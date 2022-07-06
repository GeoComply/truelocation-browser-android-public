package org.mozilla.fenix.welcome

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import androidx.appcompat.widget.Toolbar
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import org.mozilla.fenix.R

class TermOfServiceFragment : Fragment(R.layout.term_of_service_fragment) {
    @SuppressLint("SetJavaScriptEnabled")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val toolbar = view.findViewById<Toolbar>(R.id.toolbar)
        toolbar.setNavigationOnClickListener {
            navigation.navigate(R.id.action_termofservice_to_welcome1)
        }
        val webview = view.findViewById<WebView>(R.id.webview)
        webview.settings.javaScriptEnabled = true
        webview.loadUrl("https://www.geocomply.com/terms-of-use/")
        super.onViewCreated(view, savedInstanceState)
    }

    lateinit var navigation : NavController

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        navigation = findNavController()
        return super.onCreateView(inflater, container, savedInstanceState)
    }
}