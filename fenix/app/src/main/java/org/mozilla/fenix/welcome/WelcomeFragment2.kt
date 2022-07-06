package org.mozilla.fenix.welcome

import android.Manifest
import android.content.Context
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.widget.AppCompatButton
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import com.google.android.gms.common.api.ResolvableApiException
import com.google.android.gms.location.*
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Util
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.LocationSettingsRequest
import com.google.android.gms.tasks.OnFailureListener




class WelcomeFragment2 : Fragment(R.layout.fragment_welcome_2){
    lateinit var navigation: NavController
    private val requestPermission =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            navigation.navigate(R.id.action_welcome2_to_welcome3)
        }

    private val requestLocationService = registerForActivityResult(ActivityResultContracts.StartIntentSenderForResult()) {
        if (!checkPermission()) {
            //Permission not granted
            requestPermission()
        } else {
            //Permission is granted
            navigation.navigate(R.id.action_welcome2_to_welcome3)
        }
    }

    private val REQUIRED_PERMISSIONS = arrayOf(
        Manifest.permission.ACCESS_FINE_LOCATION,
        Manifest.permission.ACCESS_COARSE_LOCATION
    )

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        navigation = findNavController()
        return super.onCreateView(inflater, container, savedInstanceState)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<AppCompatButton>(R.id.next_button).setOnClickListener {
            if (!isLocationEnabled()) {
                //Location Service is Disable
                createLocationRequest()
            } else if (!checkPermission()) {
                //Permission not granted
                requestPermission()
            } else {
                //Permission is granted
                navigation.navigate(R.id.action_welcome2_to_welcome3)
            }
        }

        //Show required permission
        val permissionText =
            when {
                Util.hasS() -> {
                    R.string.ask_permission_android_12
                }
                Util.hasQ() -> {
                    R.string.ask_permission_android_10
                }
                else -> {
                    R.string.ask_permission
                }
            }
        view.findViewById<TextView>(R.id.permission_text).setText(permissionText)
    }

    /*
    Verify Location Permission is granted.
     */
    private fun checkPermission(): Boolean {
        try {
            if (Util.hasMarshmallow() && Util.wasRequiredPermissionsGranted(
                    requireContext(),
                    REQUIRED_PERMISSIONS
                )
            ) {
                return false
            }
        } catch (e: Exception) {
            return true
        }
        return true
    }

    private fun requestPermission() {
        requestPermission.launch(REQUIRED_PERMISSIONS)
    }


    private fun isLocationEnabled(): Boolean {
        try {
            val locationManager =
                requireContext().getSystemService(Context.LOCATION_SERVICE) as LocationManager
            return when {
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.P -> {
                    // This is a new method provided in API 28
                    Log.e("DEBUG", "isLocationEnabled: " + locationManager.isLocationEnabled)
                    locationManager.isLocationEnabled
                }
                else -> {

                    // This was deprecated in API 28
                    val mode = Settings.Secure.getInt(
                        requireContext().contentResolver, "location_mode",
                        Settings.Secure.LOCATION_MODE_OFF
                    )
                    mode != Settings.Secure.LOCATION_MODE_OFF
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            return true
        }
    }

    private fun createLocationRequest() {
        val locationRequest = LocationRequest.create()
        locationRequest.interval = 60000
        locationRequest.fastestInterval = 50000
        locationRequest.priority = LocationRequest.PRIORITY_HIGH_ACCURACY
        val builder = LocationSettingsRequest.Builder()
            .addLocationRequest(locationRequest)
        val client = LocationServices.getSettingsClient(requireActivity())
        val task = client.checkLocationSettings(builder.build())
        task.addOnFailureListener(requireActivity(), OnFailureListener { e ->
            Log.e("DEBUG", "addOnFailureListener: ")
            if (e is ResolvableApiException) {
                // Location settings are not satisfied, but this can be fixed
                // by showing the user a dialog.
                try {
                    // Show the dialog by calling startResolutionForResult(),
                    // and check the result in onActivityResult().
                    requestLocationService.launch(IntentSenderRequest.Builder(e.resolution).build())
                } catch (sendEx: Exception) {
                    if (!checkPermission()) {
                        //Permission not granted
                        requestPermission()
                    } else {
                        //Permission is granted
                        navigation.navigate(R.id.action_welcome2_to_welcome3)
                    }
                }
            }
        })
    }
}