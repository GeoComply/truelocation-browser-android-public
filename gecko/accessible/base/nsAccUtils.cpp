/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccUtils.h"

#include "LocalAccessible-inl.h"
#include "AccAttributes.h"
#include "ARIAMap.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "DocAccessibleParent.h"
#include "HyperTextAccessible.h"
#include "nsIAccessibleTypes.h"
#include "Role.h"
#include "States.h"
#include "TextLeafAccessible.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMXULContainerElement.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/a11y/PDocAccessibleChild.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsAccessibilityService.h"

using namespace mozilla;
using namespace mozilla::a11y;

void nsAccUtils::SetAccGroupAttrs(AccAttributes* aAttributes, int32_t aLevel,
                                  int32_t aSetSize, int32_t aPosInSet) {
  nsAutoString value;

  if (aLevel) {
    aAttributes->SetAttribute(nsGkAtoms::level, aLevel);
  }

  if (aSetSize && aPosInSet) {
    aAttributes->SetAttribute(nsGkAtoms::posinset, aPosInSet);
    aAttributes->SetAttribute(nsGkAtoms::setsize, aSetSize);
  }
}

int32_t nsAccUtils::GetLevelForXULContainerItem(nsIContent* aContent) {
  nsCOMPtr<nsIDOMXULContainerItemElement> item =
      aContent->AsElement()->AsXULContainerItem();
  if (!item) return 0;

  nsCOMPtr<dom::Element> containerElement;
  item->GetParentContainer(getter_AddRefs(containerElement));
  nsCOMPtr<nsIDOMXULContainerElement> container =
      containerElement ? containerElement->AsXULContainer() : nullptr;
  if (!container) return 0;

  // Get level of the item.
  int32_t level = -1;
  while (container) {
    level++;

    container->GetParentContainer(getter_AddRefs(containerElement));
    container = containerElement ? containerElement->AsXULContainer() : nullptr;
  }

  return level;
}

void nsAccUtils::SetLiveContainerAttributes(AccAttributes* aAttributes,
                                            nsIContent* aStartContent) {
  nsAutoString live, relevant, busy;
  dom::Document* doc = aStartContent->GetComposedDoc();
  if (!doc) {
    return;
  }
  dom::Element* topEl = doc->GetRootElement();
  nsIContent* ancestor = aStartContent;
  while (ancestor) {
    // container-relevant attribute
    if (relevant.IsEmpty() &&
        HasDefinedARIAToken(ancestor, nsGkAtoms::aria_relevant) &&
        ancestor->AsElement()->GetAttr(kNameSpaceID_None,
                                       nsGkAtoms::aria_relevant, relevant)) {
      aAttributes->SetAttributeStringCopy(nsGkAtoms::containerRelevant,
                                          relevant);
    }

    // container-live, and container-live-role attributes
    if (live.IsEmpty()) {
      const nsRoleMapEntry* role = nullptr;
      if (ancestor->IsElement()) {
        role = aria::GetRoleMap(ancestor->AsElement());
      }
      if (HasDefinedARIAToken(ancestor, nsGkAtoms::aria_live)) {
        ancestor->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_live,
                                       live);
      } else if (role) {
        GetLiveAttrValue(role->liveAttRule, live);
      } else if (nsStaticAtom* value = GetAccService()->MarkupAttribute(
                     ancestor, nsGkAtoms::aria_live)) {
        value->ToString(live);
      }

      if (!live.IsEmpty()) {
        aAttributes->SetAttributeStringCopy(nsGkAtoms::containerLive, live);
        if (role) {
          aAttributes->SetAttribute(nsGkAtoms::containerLiveRole,
                                    role->roleAtom);
        }
      }
    }

    // container-atomic attribute
    if (ancestor->IsElement() && ancestor->AsElement()->AttrValueIs(
                                     kNameSpaceID_None, nsGkAtoms::aria_atomic,
                                     nsGkAtoms::_true, eCaseMatters)) {
      aAttributes->SetAttribute(nsGkAtoms::containerAtomic, true);
    }

    // container-busy attribute
    if (busy.IsEmpty() && HasDefinedARIAToken(ancestor, nsGkAtoms::aria_busy) &&
        ancestor->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_busy,
                                       busy)) {
      aAttributes->SetAttributeStringCopy(nsGkAtoms::containerBusy, busy);
    }

    if (ancestor == topEl) {
      break;
    }

    ancestor = ancestor->GetParent();
    if (!ancestor) {
      ancestor = topEl;  // Use <body>/<frameset>
    }
  }
}

bool nsAccUtils::HasDefinedARIAToken(nsIContent* aContent, nsAtom* aAtom) {
  NS_ASSERTION(aContent, "aContent is null in call to HasDefinedARIAToken!");

  if (!aContent->IsElement()) return false;

  dom::Element* element = aContent->AsElement();
  if (!element->HasAttr(kNameSpaceID_None, aAtom) ||
      element->AttrValueIs(kNameSpaceID_None, aAtom, nsGkAtoms::_empty,
                           eCaseMatters) ||
      element->AttrValueIs(kNameSpaceID_None, aAtom, nsGkAtoms::_undefined,
                           eCaseMatters)) {
    return false;
  }
  return true;
}

nsStaticAtom* nsAccUtils::GetARIAToken(dom::Element* aElement, nsAtom* aAttr) {
  if (!HasDefinedARIAToken(aElement, aAttr)) return nsGkAtoms::_empty;

  static dom::Element::AttrValuesArray tokens[] = {
      nsGkAtoms::_false, nsGkAtoms::_true, nsGkAtoms::mixed, nullptr};

  int32_t idx =
      aElement->FindAttrValueIn(kNameSpaceID_None, aAttr, tokens, eCaseMatters);
  if (idx >= 0) return tokens[idx];

  return nullptr;
}

nsStaticAtom* nsAccUtils::NormalizeARIAToken(dom::Element* aElement,
                                             nsAtom* aAttr) {
  if (!HasDefinedARIAToken(aElement, aAttr)) {
    return nsGkAtoms::_empty;
  }

  if (aAttr == nsGkAtoms::aria_current) {
    static dom::Element::AttrValuesArray tokens[] = {
        nsGkAtoms::page, nsGkAtoms::step, nsGkAtoms::location_,
        nsGkAtoms::date, nsGkAtoms::time, nsGkAtoms::_true,
        nullptr};
    int32_t idx = aElement->FindAttrValueIn(kNameSpaceID_None, aAttr, tokens,
                                            eCaseMatters);
    // If the token is present, return it, otherwise TRUE as per spec.
    return (idx >= 0) ? tokens[idx] : nsGkAtoms::_true;
  }

  return nullptr;
}

LocalAccessible* nsAccUtils::GetSelectableContainer(
    LocalAccessible* aAccessible, uint64_t aState) {
  if (!aAccessible) return nullptr;

  if (!(aState & states::SELECTABLE)) return nullptr;

  LocalAccessible* parent = aAccessible;
  while ((parent = parent->LocalParent()) && !parent->IsSelect()) {
    if (parent->Role() == roles::PANE) return nullptr;
  }
  return parent;
}

bool nsAccUtils::IsDOMAttrTrue(const LocalAccessible* aAccessible,
                               nsAtom* aAttr) {
  dom::Element* el = aAccessible->Elm();
  return el && el->AttrValueIs(kNameSpaceID_None, aAttr, nsGkAtoms::_true,
                               eCaseMatters);
}

Accessible* nsAccUtils::TableFor(Accessible* aRow) {
  if (aRow) {
    Accessible* table = aRow->Parent();
    if (table) {
      roles::Role tableRole = table->Role();
      const nsRoleMapEntry* roleMapEntry = table->ARIARoleMap();
      if (tableRole == roles::GROUPING ||  // if there's a rowgroup.
          (table->IsGenericHyperText() && !roleMapEntry &&
           !table->IsTable())) {  // or there is a wrapping text container
        table = table->Parent();
        if (table) tableRole = table->Role();
      }

      return (tableRole == roles::TABLE || tableRole == roles::TREE_TABLE ||
              tableRole == roles::MATHML_TABLE)
                 ? table
                 : nullptr;
    }
  }

  return nullptr;
}

LocalAccessible* nsAccUtils::TableFor(LocalAccessible* aRow) {
  Accessible* table = TableFor(static_cast<Accessible*>(aRow));
  return table ? table->AsLocal() : nullptr;
}

HyperTextAccessible* nsAccUtils::GetTextContainer(nsINode* aNode) {
  // Get text accessible containing the result node.
  DocAccessible* doc = GetAccService()->GetDocAccessible(aNode->OwnerDoc());
  LocalAccessible* accessible =
      doc ? doc->GetAccessibleOrContainer(aNode) : nullptr;
  if (!accessible) return nullptr;

  do {
    HyperTextAccessible* textAcc = accessible->AsHyperText();
    if (textAcc) return textAcc;

    accessible = accessible->LocalParent();
  } while (accessible);

  return nullptr;
}

LayoutDeviceIntPoint nsAccUtils::ConvertToScreenCoords(
    int32_t aX, int32_t aY, uint32_t aCoordinateType, Accessible* aAccessible) {
  LayoutDeviceIntPoint coords(aX, aY);

  switch (aCoordinateType) {
    // Regardless of coordinate type, the coords returned
    // are in dev pixels.
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE: {
      coords += GetScreenCoordsForWindow(aAccessible);
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE: {
      coords += GetScreenCoordsForParent(aAccessible);
      break;
    }

    default:
      MOZ_ASSERT_UNREACHABLE("invalid coord type!");
  }

  return coords;
}

void nsAccUtils::ConvertScreenCoordsTo(int32_t* aX, int32_t* aY,
                                       uint32_t aCoordinateType,
                                       Accessible* aAccessible) {
  switch (aCoordinateType) {
    // Regardless of coordinate type, the values returned for
    // aX and aY are in dev pixels.
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE: {
      LayoutDeviceIntPoint coords = GetScreenCoordsForWindow(aAccessible);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE: {
      LayoutDeviceIntPoint coords = GetScreenCoordsForParent(aAccessible);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    default:
      MOZ_ASSERT_UNREACHABLE("invalid coord type!");
  }
}

LayoutDeviceIntPoint nsAccUtils::GetScreenCoordsForParent(
    Accessible* aAccessible) {
  if (!aAccessible) return LayoutDeviceIntPoint();

  if (Accessible* parent = aAccessible->Parent()) {
    LayoutDeviceIntRect parentBounds = parent->Bounds();
    // The rect returned from Bounds() is already in dev
    // pixels, so we don't need to do any conversion here.
    return parentBounds.TopLeft();
  }

  return LayoutDeviceIntPoint();
}

LayoutDeviceIntPoint nsAccUtils::GetScreenCoordsForWindow(
    Accessible* aAccessible) {
  a11y::LocalAccessible* localAcc = aAccessible->AsLocal();
  if (!localAcc) {
    localAcc = aAccessible->AsRemote()->OuterDocOfRemoteBrowser();
  }

  LayoutDeviceIntPoint coords(0, 0);
  nsCOMPtr<nsIDocShellTreeItem> treeItem(
      nsCoreUtils::GetDocShellFor(localAcc->GetNode()));
  if (!treeItem) return coords;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) return coords;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  if (baseWindow) {
    baseWindow->GetPosition(&coords.x, &coords.y);  // in device pixels
  }

  return coords;
}

bool nsAccUtils::GetLiveAttrValue(uint32_t aRule, nsAString& aValue) {
  switch (aRule) {
    case eOffLiveAttr:
      aValue = u"off"_ns;
      return true;
    case ePoliteLiveAttr:
      aValue = u"polite"_ns;
      return true;
    case eAssertiveLiveAttr:
      aValue = u"assertive"_ns;
      return true;
  }

  return false;
}

#ifdef DEBUG

bool nsAccUtils::IsTextInterfaceSupportCorrect(LocalAccessible* aAccessible) {
  // Don't test for accessible docs, it makes us create accessibles too
  // early and fire mutation events before we need to
  if (aAccessible->IsDoc()) return true;

  bool foundText = false;
  uint32_t childCount = aAccessible->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    LocalAccessible* child = aAccessible->LocalChildAt(childIdx);
    if (child && child->IsText()) {
      foundText = true;
      break;
    }
  }

  return !foundText || aAccessible->IsHyperText();
}
#endif

uint32_t nsAccUtils::TextLength(Accessible* aAccessible) {
  if (!aAccessible->IsText()) {
    return 1;
  }

  if (LocalAccessible* localAcc = aAccessible->AsLocal()) {
    TextLeafAccessible* textLeaf = localAcc->AsTextLeaf();
    if (textLeaf) {
      return textLeaf->Text().Length();
    }
  } else if (aAccessible->IsText()) {
    MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup(),
               "Shouldn't be called on a RemoteAccessible unless the cache is "
               "enabled");
    RemoteAccessible* remoteAcc = aAccessible->AsRemote();
    MOZ_ASSERT(remoteAcc);
    return remoteAcc->GetCachedTextLength();
  }

  // For list bullets (or anything other accessible which would compute its own
  // text. They don't have their own frame.
  // XXX In the future, list bullets may have frame and anon content, so
  // we should be able to remove this at that point
  nsAutoString text;
  aAccessible->AppendTextTo(text);  // Get all the text
  return text.Length();
}

bool nsAccUtils::MustPrune(Accessible* aAccessible) {
  MOZ_ASSERT(aAccessible);
  roles::Role role = aAccessible->Role();

  if (role == roles::SLIDER) {
    // Always prune the tree for sliders, as it doesn't make sense for a
    // slider to have descendants and this confuses NVDA.
    return true;
  }

  if (role != roles::MENUITEM && role != roles::COMBOBOX_OPTION &&
      role != roles::OPTION && role != roles::ENTRY &&
      role != roles::FLAT_EQUATION && role != roles::PASSWORD_TEXT &&
      role != roles::PUSHBUTTON && role != roles::TOGGLE_BUTTON &&
      role != roles::GRAPHIC && role != roles::PROGRESSBAR &&
      role != roles::SEPARATOR) {
    // If it doesn't match any of these roles, don't prune its children.
    return false;
  }

  if (aAccessible->ChildCount() != 1) {
    // If the accessible has more than one child, don't prune it.
    return false;
  }

  roles::Role childRole = aAccessible->FirstChild()->Role();
  // If the accessible's child is a text leaf, prune the accessible.
  return childRole == roles::TEXT_LEAF || childRole == roles::STATICTEXT;
}

bool nsAccUtils::IsARIALive(const LocalAccessible* aAccessible) {
  // Get computed aria-live property based on the closest container with the
  // attribute. Inner nodes override outer nodes within the same
  // document.
  // This should be the same as the container-live attribute, but we don't need
  // the other container-* attributes, so we can't use the same function.
  nsIContent* ancestor = aAccessible->GetContent();
  if (!ancestor) {
    return false;
  }
  dom::Document* doc = ancestor->GetComposedDoc();
  if (!doc) {
    return false;
  }
  dom::Element* topEl = doc->GetRootElement();
  while (ancestor) {
    const nsRoleMapEntry* role = nullptr;
    if (ancestor->IsElement()) {
      role = aria::GetRoleMap(ancestor->AsElement());
    }
    nsAutoString live;
    if (HasDefinedARIAToken(ancestor, nsGkAtoms::aria_live)) {
      ancestor->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_live,
                                     live);
    } else if (role) {
      GetLiveAttrValue(role->liveAttRule, live);
    } else if (nsStaticAtom* value = GetAccService()->MarkupAttribute(
                   ancestor, nsGkAtoms::aria_live)) {
      value->ToString(live);
    }
    if (!live.IsEmpty() && !live.EqualsLiteral("off")) {
      return true;
    }

    if (ancestor == topEl) {
      break;
    }

    ancestor = ancestor->GetParent();
    if (!ancestor) {
      ancestor = topEl;  // Use <body>/<frameset>
    }
  }

  return false;
}

Accessible* nsAccUtils::DocumentFor(Accessible* aAcc) {
  if (!aAcc) {
    return nullptr;
  }
  if (LocalAccessible* localAcc = aAcc->AsLocal()) {
    return localAcc->Document();
  }
  return aAcc->AsRemote()->Document();
}

Accessible* nsAccUtils::GetAccessibleByID(Accessible* aDoc, uint64_t aID) {
  if (!aDoc) {
    return nullptr;
  }
  if (LocalAccessible* localAcc = aDoc->AsLocal()) {
    if (DocAccessible* doc = localAcc->AsDoc()) {
      return doc->GetAccessibleByUniqueID(
          reinterpret_cast<void*>(static_cast<uintptr_t>(aID)));
    }
  } else if (DocAccessibleParent* doc = aDoc->AsRemote()->AsDoc()) {
    return doc->GetAccessible(aID);
  }
  return nullptr;
}
