/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/PresShell.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"

#define SCROLL_TIMELINE_DURATION_MILLISEC 100000

namespace mozilla::dom {

// ---------------------------------
// Methods of ScrollTimeline
// ---------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(ScrollTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScrollTimeline,
                                                AnimationTimeline)
  tmp->Teardown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScrollTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ScrollTimeline,
                                               AnimationTimeline)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ScrollTimeline,
                                               AnimationTimeline)

TimingParams ScrollTimeline::sTiming;

ScrollTimeline::ScrollTimeline(Document* aDocument, const Scroller& aScroller,
                               StyleScrollAxis aAxis)
    : AnimationTimeline(aDocument->GetParentObject()),
      mDocument(aDocument),
      mSource(aScroller),
      mAxis(aAxis) {
  MOZ_ASSERT(aDocument);

  // Use default values except for |mDuration| and |mFill|.
  // Use a fixed duration defined in SCROLL_TIMELINE_DURATIONMILLISEC, and use
  // FillMode::Both to make sure the animation is in effect at 100%.
  sTiming = TimingParams(SCROLL_TIMELINE_DURATION_MILLISEC, 0.0,
                         std::numeric_limits<float>::infinity(),
                         PlaybackDirection::Alternate, FillMode::Both);
}

static StyleScrollAxis ToStyleScrollAxis(
    const StyleScrollDirection aDirection) {
  switch (aDirection) {
    // The spec defines auto, but there is a spec issue:
    // "ISSUE 5 Define these values." in this section. The DOM interface removed
    // auto and use block as default value, so we treat auto as block now.
    // https://drafts.csswg.org/scroll-animations-1/#descdef-scroll-timeline-orientation
    case StyleScrollDirection::Auto:
    case StyleScrollDirection::Block:
      return StyleScrollAxis::Block;
    case StyleScrollDirection::Inline:
      return StyleScrollAxis::Inline;
    case StyleScrollDirection::Horizontal:
      return StyleScrollAxis::Horizontal;
    case StyleScrollDirection::Vertical:
      return StyleScrollAxis::Vertical;
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported StyleScrollDirection");
  return StyleScrollAxis::Block;
}

already_AddRefed<ScrollTimeline> ScrollTimeline::FromRule(
    const RawServoScrollTimelineRule& aRule, Document* aDocument,
    const NonOwningAnimationTarget& aTarget) {
  // Note: If the rules changes after we build the scroll-timeline rule, we
  // rebuild all CSS animtions, and then try to look up the scroll-timeline by
  // the new source and the new direction. If we cannot find a specific
  // timeline, we create one, and the unused scroll-timeline object will be
  // dropped automatically becuase no animation owns it and its ref-count
  // becomes zero.

  StyleScrollAxis axis =
      ToStyleScrollAxis(Servo_ScrollTimelineRule_GetOrientation(&aRule));

  RefPtr<ScrollTimeline> timeline;
  auto autoScroller = Scroller::Root(aTarget.mElement->OwnerDoc());
  auto* set =
      ScrollTimelineSet::GetOrCreateScrollTimelineSet(autoScroller.mElement);
  auto p = set->LookupForAdd(axis);
  if (!p) {
    timeline = new ScrollTimeline(aDocument, autoScroller, axis);
    set->Add(p, axis, timeline);
  } else {
    timeline = p->value();
  }
  return timeline.forget();
}

/* static */
already_AddRefed<ScrollTimeline> ScrollTimeline::FromAnonymousScroll(
    Document* aDocument, const NonOwningAnimationTarget& aTarget,
    StyleScrollAxis aAxis, StyleScroller aScroller) {
  MOZ_ASSERT(aTarget);
  Scroller scroller;
  switch (aScroller) {
    case StyleScroller::Root:
      scroller = Scroller::Root(aTarget.mElement->OwnerDoc());
      break;
    case StyleScroller::Nearest: {
      Element* curr = aTarget.mElement->GetFlattenedTreeParentElement();
      Element* root = aTarget.mElement->OwnerDoc()->GetDocumentElement();
      while (curr && curr != root) {
        const ComputedStyle* style = Servo_Element_GetMaybeOutOfDateStyle(curr);
        MOZ_ASSERT(style, "The ancestor should be styled.");
        if (style->StyleDisplay()->IsScrollableOverflow()) {
          break;
        }
        curr = curr->GetFlattenedTreeParentElement();
      }
      // If there is no scroll container, we use root.
      scroller = Scroller::Nearest(curr ? curr : root);
    }
  }

  RefPtr<ScrollTimeline> timeline;
  auto* set =
      ScrollTimelineSet::GetOrCreateScrollTimelineSet(scroller.mElement);
  auto p = set->LookupForAdd(aAxis);
  if (!p) {
    timeline = new ScrollTimeline(aDocument, scroller, aAxis);
    set->Add(p, aAxis, timeline);
  } else {
    timeline = p->value();
  }
  return timeline.forget();
}

Nullable<TimeDuration> ScrollTimeline::GetCurrentTimeAsDuration() const {
  // If no layout box, this timeline is inactive.
  if (!mSource || !mSource.mElement->GetPrimaryFrame()) {
    return nullptr;
  }

  // if this is not a scroller container, this timeline is inactive.
  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  if (!scrollFrame) {
    return nullptr;
  }

  const auto orientation = Axis();

  // If this orientation is not ready for scrolling (i.e. the scroll range is
  // not larger than or equal to one device pixel), we make it 100%.
  if (!scrollFrame->GetAvailableScrollingDirections().contains(orientation)) {
    return TimeDuration::FromMilliseconds(SCROLL_TIMELINE_DURATION_MILLISEC);
  }

  const nsPoint& scrollOffset = scrollFrame->GetScrollPosition();
  const nsRect& scrollRange = scrollFrame->GetScrollRange();
  const bool isHorizontal = orientation == layers::ScrollDirection::eHorizontal;

  // Note: For RTL, scrollOffset.x or scrollOffset.y may be negative, e.g. the
  // range of its value is [0, -range], so we have to use the absolute value.
  double position = std::abs(isHorizontal ? scrollOffset.x : scrollOffset.y);
  double range = isHorizontal ? scrollRange.width : scrollRange.height;
  MOZ_ASSERT(range > 0.0);
  // Use the definition of interval progress to compute the progress.
  // Note: We simplify the scroll offsets to [0%, 100%], so offset weight and
  // offset index are ignored here.
  // https://drafts.csswg.org/scroll-animations-1/#progress-calculation-algorithm
  double progress = position / range;
  return TimeDuration::FromMilliseconds(progress *
                                        SCROLL_TIMELINE_DURATION_MILLISEC);
}

layers::ScrollDirection ScrollTimeline::Axis() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const WritingMode wm = mSource.mElement->GetPrimaryFrame()->GetWritingMode();
  return mAxis == StyleScrollAxis::Horizontal ||
                 (!wm.IsVertical() && mAxis == StyleScrollAxis::Inline) ||
                 (wm.IsVertical() && mAxis == StyleScrollAxis::Block)
             ? layers::ScrollDirection::eHorizontal
             : layers::ScrollDirection::eVertical;
}

StyleOverflow ScrollTimeline::SourceScrollStyle() const {
  MOZ_ASSERT(mSource && mSource.mElement->GetPrimaryFrame());

  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  MOZ_ASSERT(scrollFrame);

  const ScrollStyles scrollStyles = scrollFrame->GetScrollStyles();

  return Axis() == layers::ScrollDirection::eHorizontal
             ? scrollStyles.mHorizontal
             : scrollStyles.mVertical;
}

bool ScrollTimeline::APZIsActiveForSource() const {
  MOZ_ASSERT(mSource);
  return gfxPlatform::AsyncPanZoomEnabled() &&
         !nsLayoutUtils::ShouldDisableApzForElement(mSource.mElement) &&
         DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(mSource.mElement);
}

bool ScrollTimeline::ScrollingDirectionIsAvailable() const {
  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  MOZ_ASSERT(scrollFrame);
  return scrollFrame->GetAvailableScrollingDirections().contains(Axis());
}

void ScrollTimeline::UnregisterFromScrollSource() {
  if (!mSource) {
    return;
  }

  if (ScrollTimelineSet* scrollTimelineSet =
          ScrollTimelineSet::GetScrollTimelineSet(mSource.mElement)) {
    scrollTimelineSet->Remove(mAxis);
    if (scrollTimelineSet->IsEmpty()) {
      ScrollTimelineSet::DestroyScrollTimelineSet(mSource.mElement);
    }
  }
}

const nsIScrollableFrame* ScrollTimeline::GetScrollFrame() const {
  if (!mSource) {
    return nullptr;
  }

  switch (mSource.mType) {
    case StyleScroller::Root:
      if (const PresShell* presShell =
              mSource.mElement->OwnerDoc()->GetPresShell()) {
        return presShell->GetRootScrollFrameAsScrollable();
      }
      return nullptr;
    case StyleScroller::Nearest:
      return nsLayoutUtils::FindScrollableFrameFor(mSource.mElement);
  }

  MOZ_ASSERT_UNREACHABLE("Unsupported scroller type");
  return nullptr;
}

// ---------------------------------
// Methods of ScrollTimelineSet
// ---------------------------------

/* static */ ScrollTimelineSet* ScrollTimelineSet::GetScrollTimelineSet(
    Element* aElement) {
  return aElement ? static_cast<ScrollTimelineSet*>(aElement->GetProperty(
                        nsGkAtoms::scrollTimelinesProperty))
                  : nullptr;
}

/* static */ ScrollTimelineSet* ScrollTimelineSet::GetOrCreateScrollTimelineSet(
    Element* aElement) {
  MOZ_ASSERT(aElement);
  ScrollTimelineSet* scrollTimelineSet = GetScrollTimelineSet(aElement);
  if (scrollTimelineSet) {
    return scrollTimelineSet;
  }

  scrollTimelineSet = new ScrollTimelineSet();
  nsresult rv = aElement->SetProperty(
      nsGkAtoms::scrollTimelinesProperty, scrollTimelineSet,
      nsINode::DeleteProperty<ScrollTimelineSet>, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("SetProperty failed");
    delete scrollTimelineSet;
    return nullptr;
  }
  return scrollTimelineSet;
}

/* static */ void ScrollTimelineSet::DestroyScrollTimelineSet(
    Element* aElement) {
  aElement->RemoveProperty(nsGkAtoms::scrollTimelinesProperty);
}

}  // namespace mozilla::dom
