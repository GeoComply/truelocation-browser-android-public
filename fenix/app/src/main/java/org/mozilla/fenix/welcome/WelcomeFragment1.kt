package org.mozilla.fenix.welcome

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.widget.AppCompatButton
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import org.mozilla.fenix.R

class WelcomeFragment1 : Fragment(R.layout.fragment_welcome_1) {

    lateinit var navigation : NavController

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<AppCompatButton>(R.id.next_button).setOnClickListener {
            navigation.navigate(R.id.action_welcome1_to_welcome2)
        }
        view.findViewById<TextView>(R.id.tvTermOfService).setOnClickListener{
            navigation.navigate(R.id.action_welcome1_to_termofservice)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        navigation = findNavController()
        return super.onCreateView(inflater, container, savedInstanceState)
    }
}

