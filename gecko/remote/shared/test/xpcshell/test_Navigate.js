/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const {
  ProgressListener,
  waitForInitialNavigationCompleted,
} = ChromeUtils.import("chrome://remote/content/shared/Navigate.jsm");

const CURRENT_URI = Services.io.newURI("http://foo.bar/");
const INITIAL_URI = Services.io.newURI("about:blank");
const TARGET_URI = Services.io.newURI("http://foo.cheese/");
const TARGET_URI_WITH_HASH = Services.io.newURI("http://foo.cheese/#foo");

class MockRequest {
  constructor(uri) {
    this.originalURI = uri;
  }

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIRequest", "nsIChannel"]);
  }
}

class MockWebProgress {
  constructor(browsingContext) {
    this.browsingContext = browsingContext;

    this.documentRequest = null;
    this.isLoadingDocument = false;
    this.listener = null;
    this.progressListenerRemoved = false;
  }

  addProgressListener(listener) {
    if (this.listener) {
      throw new Error("Cannot register listener twice");
    }

    this.listener = listener;
  }

  removeProgressListener(listener) {
    if (listener === this.listener) {
      this.listener = null;
      this.progressListenerRemoved = true;
    } else {
      throw new Error("Unknown listener");
    }
  }

  sendStartState(options = {}) {
    const { coop = false, isInitial = false } = options;

    if (coop) {
      this.browsingContext = new MockTopContext(this);
    }

    if (!this.browsingContext.currentWindowGlobal) {
      this.browsingContext.currentWindowGlobal = {};
    }

    this.browsingContext.currentWindowGlobal.isInitialDocument = isInitial;

    this.isLoadingDocument = true;
    const uri = isInitial ? INITIAL_URI : TARGET_URI;
    this.documentRequest = new MockRequest(uri);

    this.listener?.onStateChange(
      this,
      this.documentRequest,
      Ci.nsIWebProgressListener.STATE_START,
      null
    );

    return new Promise(executeSoon);
  }

  sendStopState() {
    this.browsingContext.currentURI = this.documentRequest.originalURI;

    this.isLoadingDocument = false;
    this.documentRequest = null;

    this.listener?.onStateChange(
      this,
      this.documentRequest,
      Ci.nsIWebProgressListener.STATE_STOP,
      null
    );

    return new Promise(executeSoon);
  }

  sendLocationChangeSameDocument() {
    this.documentRequest = null;
    this.browsingContext.currentURI = TARGET_URI_WITH_HASH;

    this.listener?.onLocationChange(
      this,
      this.documentRequest,
      TARGET_URI_WITH_HASH,
      Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
    );

    return new Promise(executeSoon);
  }
}

class MockTopContext {
  constructor(webProgress = null) {
    this.currentURI = CURRENT_URI;
    this.currentWindowGlobal = { isInitialDocument: true };
    this.id = 7;
    this.top = this;
    this.webProgress = webProgress || new MockWebProgress(this);
  }
}

const hasPromiseResolved = async function(promise) {
  let resolved = false;
  promise.finally(() => (resolved = true));
  // Make sure microtasks have time to run.
  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
  return resolved;
};

add_test(
  async function test_waitForInitialNavigation_initialDocumentNoWindowGlobal() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    // In some cases there might be no window global yet.
    delete browsingContext.currentWindowGlobal;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);
    await webProgress.sendStartState({ isInitial: true });

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();
    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(
      currentURI.spec,
      INITIAL_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, INITIAL_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentNotLoaded() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    await webProgress.sendStartState({ isInitial: true });

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();
    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(
      currentURI.spec,
      INITIAL_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, INITIAL_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentLoadingAndNoAdditionalLoad() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: true });
    ok(webProgress.isLoadingDocument, "Document is loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();
    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(
      currentURI.spec,
      INITIAL_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, INITIAL_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentFinishedLoadingNoAdditionalLoad() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: true });
    await webProgress.sendStopState();

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is initial document"
    );
    equal(
      currentURI.spec,
      INITIAL_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, INITIAL_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentLoadingAndAdditionalLoad() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: true });

    ok(webProgress.isLoadingDocument, "Document is loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));

    await webProgress.sendStartState({ isInitial: false });
    await webProgress.sendStopState();

    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(
      currentURI.spec,
      TARGET_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_initialDocumentFinishedLoadingAndAdditionalLoad() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: true });
    await webProgress.sendStopState();

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));

    await webProgress.sendStartState({ isInitial: false });
    await webProgress.sendStopState();

    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(
      currentURI.spec,
      TARGET_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentNotLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);
    await webProgress.sendStartState({ isInitial: false });

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();
    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(
      currentURI.spec,
      TARGET_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentAlreadyLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: false });
    ok(webProgress.isLoadingDocument, "Document is loading");

    const navigated = waitForInitialNavigationCompleted(webProgress);

    ok(
      !(await hasPromiseResolved(navigated)),
      "waitForInitialNavigationCompleted has not resolved yet"
    );

    await webProgress.sendStopState();
    const { currentURI, targetURI } = await navigated;

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(
      currentURI.spec,
      TARGET_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(
  async function test_waitForInitialNavigation_notInitialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    await webProgress.sendStartState({ isInitial: false });
    await webProgress.sendStopState();

    ok(!webProgress.isLoadingDocument, "Document is not loading");

    const { currentURI, targetURI } = await waitForInitialNavigationCompleted(
      webProgress
    );

    ok(!webProgress.isLoadingDocument, "Document is not loading");
    ok(
      !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
      "Is not initial document"
    );
    equal(
      currentURI.spec,
      TARGET_URI.spec,
      "Expected current URI has been set"
    );
    equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

    run_next_test();
  }
);

add_test(async function test_waitForInitialNavigation_resolveWhenStarted() {
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;

  await webProgress.sendStartState({ isInitial: true });
  ok(webProgress.isLoadingDocument, "Document is already loading");

  const { currentURI, targetURI } = await waitForInitialNavigationCompleted(
    webProgress,
    {
      resolveWhenStarted: true,
    }
  );

  ok(webProgress.isLoadingDocument, "Document is still loading");
  ok(
    webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
    "Is initial document"
  );
  equal(currentURI.spec, CURRENT_URI.spec, "Expected current URI has been set");
  equal(targetURI.spec, INITIAL_URI.spec, "Expected target URI has been set");

  run_next_test();
});

add_test(async function test_waitForInitialNavigation_crossOrigin() {
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;

  ok(!webProgress.isLoadingDocument, "Document is not loading");

  const navigated = waitForInitialNavigationCompleted(webProgress);
  await webProgress.sendStartState({ coop: true });

  ok(
    !(await hasPromiseResolved(navigated)),
    "waitForInitialNavigationCompleted has not resolved yet"
  );

  await webProgress.sendStopState();
  const { currentURI, targetURI } = await navigated;

  notEqual(
    browsingContext,
    webProgress.browsingContext,
    "Got new browsing context"
  );
  ok(!webProgress.isLoadingDocument, "Document is not loading");
  ok(
    !webProgress.browsingContext.currentWindowGlobal.isInitialDocument,
    "Is not initial document"
  );
  equal(currentURI.spec, TARGET_URI.spec, "Expected current URI has been set");
  equal(targetURI.spec, TARGET_URI.spec, "Expected target URI has been set");

  run_next_test();
});

add_test(async function test_ProgressListener_expectNavigation() {
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;

  const progressListener = new ProgressListener(webProgress, {
    expectNavigation: true,
    unloadTimeout: 10,
  });
  const navigated = progressListener.start();

  // Wait for unloadTimeout to finish in case it started
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 30));

  ok(!(await hasPromiseResolved(navigated)), "Listener has not resolved yet");

  await webProgress.sendStartState();
  await webProgress.sendStopState();

  ok(await hasPromiseResolved(navigated), "Listener has resolved");

  run_next_test();
});

add_test(
  async function test_ProgressListener_expectNavigation_initialDocumentFinishedLoading() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    const progressListener = new ProgressListener(webProgress, {
      expectNavigation: true,
      unloadTimeout: 10,
    });
    const navigated = progressListener.start();

    ok(!(await hasPromiseResolved(navigated)), "Listener has not resolved yet");

    await webProgress.sendStartState({ isInitial: true });
    await webProgress.sendStopState();

    // Wait for unloadTimeout to finish in case it started
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 30));

    ok(!(await hasPromiseResolved(navigated)), "Listener has not resolved yet");

    await webProgress.sendStartState();
    await webProgress.sendStopState();

    ok(await hasPromiseResolved(navigated), "Listener has resolved");

    run_next_test();
  }
);

add_test(async function test_ProgressListener_notWaitForExplicitStart() {
  // Create a webprogress and start it before creating the progress listener.
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;
  await webProgress.sendStartState();

  // Create the progress listener for a webprogress already in a navigation.
  const progressListener = new ProgressListener(webProgress, {
    waitForExplicitStart: false,
  });
  const navigated = progressListener.start();

  // Send stop state to complete the initial navigation
  await webProgress.sendStopState();
  ok(
    await hasPromiseResolved(navigated),
    "Listener has resolved after initial navigation"
  );

  run_next_test();
});

add_test(async function test_ProgressListener_waitForExplicitStart() {
  // Create a webprogress and start it before creating the progress listener.
  const browsingContext = new MockTopContext();
  const webProgress = browsingContext.webProgress;
  await webProgress.sendStartState();

  // Create the progress listener for a webprogress already in a navigation.
  const progressListener = new ProgressListener(webProgress, {
    waitForExplicitStart: true,
  });
  const navigated = progressListener.start();

  // Send stop state to complete the initial navigation
  await webProgress.sendStopState();
  ok(
    !(await hasPromiseResolved(navigated)),
    "Listener has not resolved after initial navigation"
  );

  // Start a new navigation
  await webProgress.sendStartState();
  ok(
    !(await hasPromiseResolved(navigated)),
    "Listener has not resolved after starting new navigation"
  );

  // Finish the new navigation
  await webProgress.sendStopState();
  ok(
    await hasPromiseResolved(navigated),
    "Listener resolved after finishing the new navigation"
  );

  run_next_test();
});

add_test(
  async function test_ProgressListener_resolveWhenNavigatingInsideDocument() {
    const browsingContext = new MockTopContext();
    const webProgress = browsingContext.webProgress;

    const progressListener = new ProgressListener(webProgress);
    const navigated = progressListener.start();

    ok(!(await hasPromiseResolved(navigated)), "Listener has not resolved");

    // Send same document location change notification to complete the navigation
    await webProgress.sendLocationChangeSameDocument();

    ok(await hasPromiseResolved(navigated), "Listener has resolved");

    const { currentURI, targetURI } = progressListener;
    equal(
      currentURI.spec,
      TARGET_URI_WITH_HASH.spec,
      "Expected current URI has been set"
    );
    equal(
      targetURI.spec,
      TARGET_URI_WITH_HASH.spec,
      "Expected target URI has been set"
    );

    run_next_test();
  }
);
