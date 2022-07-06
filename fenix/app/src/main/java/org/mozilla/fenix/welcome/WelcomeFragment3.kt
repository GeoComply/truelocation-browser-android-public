package org.mozilla.fenix.welcome

import android.app.Activity
import android.os.Bundle
import android.view.View
import androidx.appcompat.widget.AppCompatButton
import androidx.fragment.app.Fragment
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Util

class WelcomeFragment3 : Fragment(R.layout.fragment_welcome_3) {
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<AppCompatButton>(R.id.next_button).setOnClickListener {
            Util.finishWelcomeScreen(requireContext())
            activity?.setResult(Activity.RESULT_OK)
            activity?.finish()
        }
    }
}