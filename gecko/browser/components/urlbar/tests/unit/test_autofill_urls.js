/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HEURISTIC_FALLBACK_PROVIDERNAME = "HeuristicFallback";
const PLACES_PROVIDERNAME = "Places";

// "example.com/foo/" should match http://example.com/foo/.
testEngine_setup();

add_task(async function multipleSlashes() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
    },
  ]);
  let context = createContext("example.com/foo/", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/foo/",
    completed: "http://example.com/foo/",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com/foo/",
        title: "example.com/foo/",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8888/f" should match http://example.com:8888/foo.
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo",
    completed: "http://example.com:8888/foo",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo",
        title: "example.com:8888/foo",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// "example.com:8999/f" should *not* autofill http://example.com:8888/foo.
add_task(async function portNoMatch() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo",
    },
  ]);
  let context = createContext("example.com:8999/f", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://example.com:8999/f",
        title: "http://example.com:8999/f",
        iconUri: "page-icon:http://example.com:8999/",
        heuristic: true,
        providerName: HEURISTIC_FALLBACK_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/b", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/",
    completed: "http://example.com:8888/foo/bar/",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/",
        title: "example.com:8888/foo/bar/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "test visit for http://example.com:8888/foo/bar/baz",
        tags: [],
        providerName: PLACES_PROVIDERNAME,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill to the next slash, end of url
add_task(async function port() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com:8888/foo/bar/baz",
    },
  ]);
  let context = createContext("example.com:8888/foo/bar/b", {
    isPrivate: false,
  });
  await check_results({
    context,
    autofilled: "example.com:8888/foo/bar/baz",
    completed: "http://example.com:8888/foo/bar/baz",
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com:8888/foo/bar/baz",
        title: "example.com:8888/foo/bar/baz",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// autofill with case insensitive from history and bookmark.
add_task(async function caseInsensitiveFromHistoryAndBookmark() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo",
    },
  ]);

  await testCaseInsensitive();

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// autofill with case insensitive from history.
add_task(async function caseInsensitiveFromHistory() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo",
    },
  ]);

  await testCaseInsensitive();

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// autofill with case insensitive from bookmark.
add_task(async function caseInsensitiveFromBookmark() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "http://example.com/foo",
  });

  await testCaseInsensitive();

  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  await cleanupPlaces();
});

// should *not* autofill if the URI fragment does not match with case-sensitive.
add_task(async function uriFragmentCaseSensitive() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/#TEST",
    },
  ]);
  const context = createContext("http://example.com/#t", { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        uri: "http://example.com/#t",
        title: "http://example.com/#t",
        heuristic: true,
      }),
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://example.com/#TEST",
        title: "test visit for http://example.com/#TEST",
        tags: [],
      }),
    ],
  });

  await cleanupPlaces();
});

// should autofill if the URI fragment matches with case-sensitive.
add_task(async function uriFragmentCaseSensitive() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/#TEST",
    },
  ]);
  const context = createContext("http://example.com/#T", { isPrivate: false });
  await check_results({
    context,
    autofilled: "http://example.com/#TEST",
    completed: "http://example.com/#TEST",
    matches: [
      makeVisitResult(context, {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        uri: "http://example.com/#TEST",
        title: "example.com/#TEST",
        heuristic: true,
      }),
    ],
  });

  await cleanupPlaces();
});

async function testCaseInsensitive() {
  const testData = [
    {
      input: "example.com/F",
      expectedAutofill: "example.com/Foo",
    },
    {
      // Test with prefix.
      input: "http://example.com/F",
      expectedAutofill: "http://example.com/Foo",
    },
  ];

  for (const { input, expectedAutofill } of testData) {
    const context = createContext(input, {
      isPrivate: false,
    });
    await check_results({
      context,
      autofilled: expectedAutofill,
      completed: "http://example.com/foo",
      matches: [
        makeVisitResult(context, {
          uri: "http://example.com/foo",
          title: "example.com/foo",
          heuristic: true,
        }),
      ],
    });
  }

  await cleanupPlaces();
}

// Checks a URL with an origin that looks like a prefix: a scheme with no dots +
// a port.
add_task(async function originLooksLikePrefix1() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://localhost:8888/foo",
    },
  ]);
  const context = createContext("localhost:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo",
    completed: "http://localhost:8888/foo",
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo",
        title: "localhost:8888/foo",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// Same as previous (originLooksLikePrefix1) but uses a URL whose path has two
// slashes, not one.
add_task(async function originLooksLikePrefix2() {
  await PlacesTestUtils.addVisits([
    {
      uri: "http://localhost:8888/foo/bar",
    },
  ]);

  let context = createContext("localhost:8888/f", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo/",
    completed: "http://localhost:8888/foo/",
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/",
        title: "localhost:8888/foo/",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/bar",
        title: "test visit for http://localhost:8888/foo/bar",
        providerName: PLACES_PROVIDERNAME,
        tags: [],
      }),
    ],
  });

  context = createContext("localhost:8888/foo/b", { isPrivate: false });
  await check_results({
    context,
    autofilled: "localhost:8888/foo/bar",
    completed: "http://localhost:8888/foo/bar",
    matches: [
      makeVisitResult(context, {
        uri: "http://localhost:8888/foo/bar",
        title: "localhost:8888/foo/bar",
        heuristic: true,
      }),
    ],
  });
  await cleanupPlaces();
});

// Checks view-source pages as a prefix
// Uses bookmark because addVisits does not allow non-http uri's
add_task(async function viewSourceAsPrefix() {
  let address = "view-source:https://www.example.com/";
  let title = "A view source bookmark";
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: address,
    title,
  });

  let testData = [
    {
      input: "view-source:h",
      expectedAutofill: "view-source:https:/",
    },
    {
      input: "view-source:http",
      expectedAutofill: "view-source:https:/",
    },
  ];

  // Only autofills from view-source:h to view-source:https:/
  for (let { input, expectedAutofill } of testData) {
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed: expectedAutofill,
      autofilled: expectedAutofill,
      matches: [
        {
          heuristic: true,
          type: UrlbarUtils.RESULT_TYPE.URL,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        },
        makeBookmarkResult(context, {
          uri: address,
          iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
          title,
        }),
      ],
    });
  }

  await cleanupPlaces();
});

// Checks data url prefixes
// Uses bookmark because addVisits does not allow non-http uri's
add_task(async function dataAsPrefix() {
  let address = "data:text/html,%3Ch1%3EHello%2C World!%3C%2Fh1%3E";
  let title = "A data url bookmark";
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: address,
    title,
  });

  let testData = [
    {
      input: "data:t",
      expectedAutofill: "data:text/",
    },
    {
      input: "data:text",
      expectedAutofill: "data:text/",
    },
  ];

  // Only autofills from text:t to view-source:https:/
  for (let { input, expectedAutofill } of testData) {
    let context = createContext(input, { isPrivate: false });
    await check_results({
      context,
      completed: expectedAutofill,
      autofilled: expectedAutofill,
      matches: [
        {
          heuristic: true,
          type: UrlbarUtils.RESULT_TYPE.URL,
          source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        },
        makeBookmarkResult(context, {
          uri: address,
          iconUri: "chrome://global/skin/icons/defaultFavicon.svg",
          title,
        }),
      ],
    });
  }

  await cleanupPlaces();
});
