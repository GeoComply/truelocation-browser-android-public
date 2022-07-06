/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_colorwaycloset_smoke_test() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "chrome://browser/content/colorwaycloset.html",
    },
    async browser => {
      const { document } = browser.contentWindow;

      ok(
        document.getElementById("collection-expiry-date"),
        "expiry date exists"
      );

      ok(
        document.getElementById("collection-title"),
        "collection title exists"
      );

      ok(
        document.getElementsByTagName("colorway-selector"),
        "Found colorway selector element"
      );

      ok(document.getElementById("colorway-name"), "colorway name exists");

      ok(
        document.getElementById("colorway-description"),
        "colorway description exists"
      );
    }
  );
});
