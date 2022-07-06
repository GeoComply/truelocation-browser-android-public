/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use glean::TimerId;
#[cfg(feature = "gecko")]
use fog::metrics::wr;
use std::time::Duration;

pub struct Telemetry;

/// Defines the interface for hooking up an external telemetry reporter to WR.
#[cfg(not(feature = "gecko"))]
impl Telemetry {
    // Start rasterize glyph time collection
    pub fn start_rasterize_glyphs_time() -> TimerId { return 0; }
    // End rasterize glyph time collection
    pub fn stop_and_accumulate_rasterize_glyphs_time(_id: TimerId) { }
    pub fn start_framebuild_time() -> TimerId { 0 }
    pub fn stop_and_accumulate_framebuild_time(_id: TimerId) { }
    pub fn record_scenebuild_time(_duration: Duration) { }
    pub fn start_sceneswap_time() -> TimerId { 0 }
    pub fn stop_and_accumulate_sceneswap_time(_id: TimerId) { }
    pub fn cancel_sceneswap_time(_id: TimerId) { }
}

#[cfg(feature = "gecko")]
impl Telemetry {
    pub fn start_rasterize_glyphs_time() -> TimerId { wr::rasterize_glyphs_time.start() }
    pub fn stop_and_accumulate_rasterize_glyphs_time(id: TimerId) { wr::rasterize_glyphs_time.stop_and_accumulate(id); }
    pub fn start_framebuild_time() -> TimerId { wr::framebuild_time.start() }
    pub fn stop_and_accumulate_framebuild_time(id: TimerId) { wr::framebuild_time.stop_and_accumulate(id); }
    pub fn record_scenebuild_time(duration: Duration) { wr::scenebuild_time.accumulate_raw_duration(duration); }
    pub fn start_sceneswap_time() -> TimerId { wr::sceneswap_time.start() }
    pub fn stop_and_accumulate_sceneswap_time(id: TimerId) { wr::sceneswap_time.stop_and_accumulate(id); }
    pub fn cancel_sceneswap_time(id: TimerId) { wr::sceneswap_time.cancel(id); }
}
