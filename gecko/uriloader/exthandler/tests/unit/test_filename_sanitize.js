/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that
// nsIMIMEService.validateFileNameForSaving sanitizes filenames
// properly with different flags.

"use strict";

add_task(async function validate_filename_method() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

  function checkFilename(filename, flags) {
    return mimeService.validateFileNameForSaving(filename, "image/png", flags);
  }

  Assert.equal(checkFilename("basicfile.png", 0), "basicfile.png");
  Assert.equal(checkFilename("  whitespace.png  ", 0), "whitespace.png");
  Assert.equal(
    checkFilename(" .whitespaceanddots.png...", 0),
    "whitespaceanddots.png"
  );
  Assert.equal(
    checkFilename("  \u00a0 \u00a0 extrawhitespace.png  \u00a0 \u00a0 ", 0),
    "extrawhitespace.png"
  );
  Assert.equal(
    checkFilename("  filename  with  whitespace.png  ", 0),
    "filename with whitespace.png"
  );
  Assert.equal(checkFilename("\\path.png", 0), "_path.png");
  Assert.equal(
    checkFilename("\\path*and/$?~file.png", 0),
    "_path and_$ ~file.png"
  );
  Assert.equal(
    checkFilename(" \u180e whit\u180ee.png \u180e", 0),
    "whit\u180ee.png"
  );
  Assert.equal(checkFilename("簡単簡単簡単", 0), "簡単簡単簡単.png");
  Assert.equal(checkFilename(" happy\u061c\u2069.png", 0), "happy__.png");
  Assert.equal(
    checkFilename("12345678".repeat(31) + "abcdefgh.png", 0),
    "12345678".repeat(31) + "abc.png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(41) + ".png", 0),
    "簡単".repeat(41) + ".png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(42) + ".png", 0),
    "簡単".repeat(41) + "簡.png"
  );
  Assert.equal(
    checkFilename("簡単".repeat(56) + ".png", 0),
    "簡単".repeat(41) + "簡.png"
  );
  Assert.equal(checkFilename("café.png", 0), "café.png");
  Assert.equal(
    checkFilename("café".repeat(50) + ".png", 0),
    "café".repeat(50) + ".png"
  );
  Assert.equal(
    checkFilename("café".repeat(51) + ".png", 0),
    "café".repeat(50) + "c.png"
  );

  Assert.equal(
    checkFilename("\u{100001}\u{100002}.png", 0),
    "\u{100001}\u{100002}.png"
  );
  Assert.equal(
    checkFilename("\u{100001}\u{100002}".repeat(31) + ".png", 0),
    "\u{100001}\u{100002}".repeat(31) + ".png"
  );
  Assert.equal(
    checkFilename("\u{100001}\u{100002}".repeat(32) + ".png", 0),
    "\u{100001}\u{100002}".repeat(31) + ".png"
  );

  Assert.equal(
    checkFilename("noextensionfile".repeat(16), 0),
    "noextensionfile".repeat(16) + ".png"
  );
  Assert.equal(
    checkFilename("noextensionfile".repeat(17), 0),
    "noextensionfile".repeat(16) + "noextension.png"
  );
  Assert.equal(
    checkFilename("noextensionfile".repeat(16) + "noextensionfil.", 0),
    "noextensionfile".repeat(16) + "noextension.png"
  );

  Assert.equal(checkFilename("  first  .png  ", 0), "first .png");
  Assert.equal(
    checkFilename(
      "  second  .png  ",
      mimeService.VALIDATE_DONT_COLLAPSE_WHITESPACE
    ),
    "second  .png"
  );

  // For whatever reason, the Android mime handler accepts the .jpeg
  // extension for image/png, so skip this test there.
  if (AppConstants.platform != "android") {
    Assert.equal(checkFilename("thi/*rd.jpeg", 0), "thi_ rd.png");
  }

  Assert.equal(
    checkFilename("f*\\ourth  file.jpg", mimeService.VALIDATE_SANITIZE_ONLY),
    "f _ourth file.jpg"
  );
  Assert.equal(
    checkFilename(
      "f*\\ift  h.jpe*\\g",
      mimeService.VALIDATE_SANITIZE_ONLY |
        mimeService.VALIDATE_DONT_COLLAPSE_WHITESPACE
    ),
    "f _ift  h.jpe _g"
  );
  Assert.equal(checkFilename("sixth.j  pe/*g", 0), "sixth.png");

  let repeatStr = "12345678".repeat(31);
  Assert.equal(
    checkFilename(
      repeatStr + "seventh.png",
      mimeService.VALIDATE_DONT_TRUNCATE
    ),
    repeatStr + "seventh.png"
  );
  Assert.equal(
    checkFilename(repeatStr + "seventh.png", 0),
    repeatStr + "sev.png"
  );
});
