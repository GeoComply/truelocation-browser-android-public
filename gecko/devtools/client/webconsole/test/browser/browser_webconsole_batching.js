/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check adding console calls as batch keep the order of the message.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-batching.html";
const { l10n } = require("devtools/client/webconsole/utils/messages");

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const messageNumber = 100;
  await testSimpleBatchLogging(hud, messageNumber);
  await testBatchLoggingAndClear(hud, messageNumber);
});

async function testSimpleBatchLogging(hud, messageNumber) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [messageNumber], function(
    numMessages
  ) {
    content.wrappedJSObject.batchLog(numMessages);
  });
  const allMessages = await waitFor(async () => {
    const msgs = await findAllMessagesVirtualized(hud);
    if (msgs.length == messageNumber) {
      return msgs;
    }
    return null;
  });
  for (let i = 0; i < messageNumber; i++) {
    const node = allMessages[i].querySelector(".message-body");
    is(
      node.textContent,
      i.toString(),
      `message at index "${i}" is the expected one`
    );
  }
}

async function testBatchLoggingAndClear(hud, messageNumber) {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [messageNumber], function(
    numMessages
  ) {
    content.wrappedJSObject.batchLogAndClear(numMessages);
  });
  await waitFor(() => findMessage(hud, l10n.getStr("consoleCleared")));
  ok(true, "console cleared message is displayed");

  // Passing the text argument as an empty string will returns all the message,
  // whatever their content is.
  const messages = findMessages(hud, "");
  is(messages.length, 1, "console was cleared as expected");
}
