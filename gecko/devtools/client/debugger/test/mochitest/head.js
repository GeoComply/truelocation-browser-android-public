/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint-disable no-unused-vars */

"use strict";

// This head.js file is only imported by debugger mochitests.
// Anything that is meant to be used by tests of other panels should be moved to shared-head.js
// Also, any symbol that may conflict with other test symbols should stay in head.js
// (like EXAMPLE_URL)

const EXAMPLE_URL =
  "https://example.com/browser/devtools/client/debugger/test/mochitest/examples/";

// This URL is remote compared to EXAMPLE_URL, as one uses .com and the other uses .org
// Note that this depends on initDebugger to always use EXAMPLE_URL
const EXAMPLE_REMOTE_URL =
  "https://example.org/browser/devtools/client/debugger/test/mochitest/examples/";

// shared-head.js handles imports, constants, and utility functions
/* import-globals-from ../../../shared/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

/* import-globals-from ./shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/shared-head.js",
  this
);

/**
 * Helper function for `_loadAllIntegrationTests`.
 *
 * Implements this as a global method in order to please eslint.
 * This will be used by modules loaded from "integration-tests" folder
 * in order to register a new integration task, ran when executing `runAllIntegrationTests`.
 */
const integrationTasks = [];
function addIntegrationTask(fun) {
  integrationTasks.push(fun);
}

/**
 * Helper function for `runAllIntegrationTests`.
 *
 * Loads all the modules from "integration-tests" folder and return
 * all the task they implemented and registered by calling `addIntegrationTask`.
 */
function _loadAllIntegrationTests() {
  const testsDir = getChromeDir(getResolvedURI(gTestPath));
  testsDir.append("integration-tests");
  const entries = testsDir.directoryEntries;
  const urls = [];
  while (entries.hasMoreElements()) {
    const file = entries.nextFile;
    const url = Services.io.newFileURI(file).spec;
    if (url.endsWith(".js")) {
      urls.push(url);
    }
  }

  // We need to sort in order to run the test in a reliable order
  urls.sort();

  for (const url of urls) {
    Services.scriptloader.loadSubScript(url, this);
  }
  return integrationTasks;
}

/**
 * Method to be called by each integration tests which will
 * run all the "integration tasks" implemented in files from the "integration-tests" folder.
 * These files should call the `addIntegrationTask()` method to register something to run.
 *
 * @param {String} testFolder
 *        Define what folder in "examples" folder to load before opening the debugger.
 *        This is meant to be a versionized test folder with v1, v2, v3 folders.
 *        (See createVersionizedHttpTestServer())
 * @param {Object} env
 *        Environment object passed down to each task to better know
 *        which particular integration test is being run.
 */
async function runAllIntegrationTests(testFolder, env) {
  const tasks = _loadAllIntegrationTests();

  const testServer = createVersionizedHttpTestServer("examples/" + testFolder);
  const testUrl = testServer.urlFor("index.html");

  for (const task of tasks) {
    info(` ==> Running integration task '${task.name}'`);
    await task(testServer, testUrl, env);
  }
}

/**
 * Install a Web Extension which will run a content script against any test page
 * served from https://example.com
 *
 * This content script is meant to be debuggable when devtools.chrome.enabled is true.
 */
async function installAndStartContentScriptExtension() {
  function contentScript() {
    console.log("content script loads");

    // This listener prevents the source from being garbage collected
    // and be missing from the scripts returned by `dbg.findScripts()`
    // in `ThreadActor._discoverSources`.
    window.onload = () => {};
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test content script extension",
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["https://example.com/*"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  return extension;
}
