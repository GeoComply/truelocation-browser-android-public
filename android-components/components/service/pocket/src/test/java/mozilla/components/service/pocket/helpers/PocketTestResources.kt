/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.api.PocketApiStory
import mozilla.components.service.pocket.stories.db.PocketStoryEntity

private const val POCKET_DIR = "pocket"

/**
 * Accessors to resources used in testing.
 */
internal object PocketTestResources {
    val pocketEndointFiveStoriesResponse = this::class.java.classLoader!!.getResource(
        "$POCKET_DIR/stories_recommendations_response.json"
    )!!.readText()

    val apiExpectedPocketStoriesRecommendations: List<PocketApiStory> = listOf(
        PocketApiStory(
            title = "How to Remember Anything You Really Want to Remember, Backed by Science",
            url = "https://getpocket.com/explore/item/how-to-remember-anything-you-really-want-to-remember-backed-by-science",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fwww.incimages.com%252Fuploaded_files%252Fimage%252F1920x1080%252Fgetty-862457080_394628.jpg",
            publisher = "Pocket",
            category = "general",
            timeToRead = 3
        ),
        PocketApiStory(
            title = "‘I Don’t Want to Be Like a Family With My Co-Workers’",
            url = "https://www.thecut.com/article/i-dont-want-to-be-like-a-family-with-my-co-workers.html",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpyxis.nymag.com%2Fv1%2Fimgs%2Fac8%2Fd22%2F315cd0cf1e3a43edfe0e0548f2edbcb1a1-ask-a-boss.1x.rsocial.w1200.jpg",
            publisher = "The Cut",
            category = "general",
            timeToRead = 5
        ),
        PocketApiStory(
            title = "How America Failed in Afghanistan",
            url = "https://www.newyorker.com/news/q-and-a/how-america-failed-in-afghanistan",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fmedia.newyorker.com%2Fphotos%2F6119484157b611aec9c99b43%2F16%3A9%2Fw_1280%2Cc_limit%2FChotiner-Afghanistan01.jpg",
            publisher = "The New Yorker",
            category = "general",
            timeToRead = 14
        ),
        PocketApiStory(
            title = "How digital beauty filters perpetuate colorism",
            url = "https://www.technologyreview.com/2021/08/15/1031804/digital-beauty-filters-photoshop-photo-editing-colorism-racism/",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fwp.technologyreview.com%2Fwp-content%2Fuploads%2F2021%2F08%2FBeautyScoreColorism.jpg%3Fresize%3D1200%2C600",
            publisher = "MIT Technology Review",
            category = "general",
            timeToRead = 11
        ),
        PocketApiStory(
            title = "How to Get Rid of Black Mold Naturally",
            url = "https://getpocket.com/explore/item/how-to-get-rid-of-black-mold-naturally",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fpocket-syndicated-images.s3.amazonaws.com%252Farticles%252F6757%252F1628024495_6109ae86db6cc.png",
            publisher = "Pocket",
            category = "general",
            timeToRead = 4
        )
    )

    val dbExpectedPocketStory = PocketStoryEntity(
        title = "How to Get Rid of Black Mold Naturally",
        url = "https://getpocket.com/explore/item/how-to-get-rid-of-black-mold-naturally",
        imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fpocket-syndicated-images.s3.amazonaws.com%252Farticles%252F6757%252F1628024495_6109ae86db6cc.png",
        publisher = "Pocket",
        category = "general",
        timeToRead = 4,
        timesShown = 23
    )

    val clientExpectedPocketStory = PocketRecommendedStory(
        title = "How digital beauty filters perpetuate colorism",
        url = "https://www.technologyreview.com/2021/08/15/1031804/digital-beauty-filters-photoshop-photo-editing-colorism-racism/",
        imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fwp.technologyreview.com%2Fwp-content%2Fuploads%2F2021%2F08%2FBeautyScoreColorism.jpg%3Fresize%3D1200%2C600",
        publisher = "MIT Technology Review",
        category = "general",
        timeToRead = 11,
        timesShown = 3
    )
}
