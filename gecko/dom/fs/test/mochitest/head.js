/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function require_module(id) {
  if (!require_module.moduleLoader) {
    const { ModuleLoader } = await import(
      "/tests/dom/quota/test/modules/ModuleLoader.js"
    );

    const base = window.location.href;

    const depth = "../../../../";

    const { Assert } = await import("/tests/dom/quota/test/modules/Assert.js");

    const proto = {
      Assert,
      Cr: SpecialPowers.Cr,
      navigator,
    };

    require_module.moduleLoader = new ModuleLoader(base, depth, proto);
  }

  return require_module.moduleLoader.require(id);
}

async function run_test_in_worker(script) {
  const { runTestInWorker } = await import(
    "/tests/dom/quota/test/modules/WorkerDriver.js"
  );
  await runTestInWorker(script);
}

// XXX It would be nice if we could call add_setup here (xpcshell-test and
//     browser-test support it.
add_task(async function() {
  const { setStoragePrefs, clearStoragesForOrigin } = await import(
    "/tests/dom/quota/test/modules/StorageUtils.js"
  );

  const optionalPrefsToSet = [["dom.fs.enabled", true]];

  await setStoragePrefs(optionalPrefsToSet);

  SimpleTest.registerCleanupFunction(async function() {
    await clearStoragesForOrigin(SpecialPowers.wrap(document).nodePrincipal);
  });
});
