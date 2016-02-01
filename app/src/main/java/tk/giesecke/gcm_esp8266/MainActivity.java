package tk.giesecke.gcm_esp8266;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.gcm.GoogleCloudMessaging;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

@SuppressWarnings("deprecation")
public class MainActivity extends AppCompatActivity implements View.OnClickListener {

	private static final String DEBUG_LOG_TAG = "GCM-ESP";

	// Registration Id from GCM
	private static final String PREF_GCM_REG_ID = "PREF_GCM_REG_ID";
	private SharedPreferences prefs;

	// Your project number and web server url.
	private static final String GCM_SENDER_ID = "928633261430";
	private static final String WEB_SERVER_URL = "http://192.168.0.148";

	private GoogleCloudMessaging gcm;
	private Button registerBtn;
	private Button unRegisterBtn;
	private Button listBtn;
	private TextView regIdView;

	private static final int ACTION_PLAY_SERVICES_DIALOG = 100;
	private static final int MSG_REGISTER_WITH_GCM = 101;
	private static final int MSG_REGISTER_WEB_SERVER = 102;
	private static final int MSG_REGISTER_WEB_SERVER_SUCCESS = 103;
	private static final int MSG_REGISTER_WEB_SERVER_FAILURE = 104;
	private String gcmRegId;

	private static Handler updateUIHandler;


	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		// Enable access to internet
		if (Build.VERSION.SDK_INT > 9) {
			/** ThreadPolicy to get permission to access internet */
			StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
			StrictMode.setThreadPolicy(policy);
		}

		registerBtn = (Button) findViewById(R.id.bt_regDevice);
		registerBtn.setOnClickListener(this);
		unRegisterBtn = (Button) findViewById(R.id.bt_unRegDevice);
		unRegisterBtn.setOnClickListener(this);
		listBtn = (Button) findViewById(R.id.bt_getRegList);
		listBtn.setOnClickListener(this);

		regIdView = (TextView) findViewById(R.id.regId);

		// Read saved registration id from shared preferences.
		gcmRegId = getSharedPreferences().getString(PREF_GCM_REG_ID, "");

		if (TextUtils.isEmpty(gcmRegId)) {
			showHideButtons(false);
		}else{
			showHideButtons(true);
			regIdView.setText(gcmRegId);
			Toast.makeText(getApplicationContext(),
					"Already registered with GCM", Toast.LENGTH_SHORT).show();
			if (BuildConfig.DEBUG)
				Log.d(DEBUG_LOG_TAG, "Already registered with GCM: " + gcmRegId);
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
		// Register handler for UI update
		updateUIHandler = new Handler();

		// Register the receiver for messages from GCM listener
		if (activityReceiver != null) {
			//Create an intent filter to listen to the broadcast sent with the action "ACTION_STRING_ACTIVITY"
			IntentFilter intentFilter = new IntentFilter(GCMIntentService.BROADCAST_RECEIVED);
			//Map the intent filter to the receiver
			registerReceiver(activityReceiver, intentFilter);
		}
	}

	@Override
	public void onPause() {
		super.onPause();
		// Unregister the receiver for messages from GCM listener
		unregisterReceiver(activityReceiver);
	}

	/**
	 * Called when a view has been clicked.
	 *
	 * @param view
	 * 		The view that was clicked.
	 */
	@Override
	public void onClick(View view) {
		switch (view.getId()) {
			case R.id.bt_regDevice:
				// Check device for Play Services APK.
				if (isGooglePlayInstalled()) {
					gcm = GoogleCloudMessaging.getInstance(getApplicationContext());

					// Read saved registration id from shared preferences.
					gcmRegId = getSharedPreferences().getString(PREF_GCM_REG_ID, "");

					if (TextUtils.isEmpty(gcmRegId)) {
						handler.sendEmptyMessage(MSG_REGISTER_WITH_GCM);
					}else{
						regIdView.setText(gcmRegId);
						Toast.makeText(getApplicationContext(),
								"Already registered with GCM", Toast.LENGTH_SHORT).show();
						if (BuildConfig.DEBUG)
							Log.d(DEBUG_LOG_TAG, "Already registered with GCM: " + gcmRegId);
					}
				}
				break;
			case R.id.bt_unRegDevice:
				new WebServerTask().execute("/?di=" + gcmRegId);
				deleteFromSharedPref();
				showHideButtons(false);
				break;
			case R.id.bt_getRegList:
				new WebServerTask().execute("/?l");
				break;
		}

	}

	private boolean isGooglePlayInstalled() {
		int resultCode = GooglePlayServicesUtil
				.isGooglePlayServicesAvailable(this);
		if (resultCode != ConnectionResult.SUCCESS) {
			if (GooglePlayServicesUtil.isUserRecoverableError(resultCode)) {
				GooglePlayServicesUtil.getErrorDialog(resultCode, this,
						ACTION_PLAY_SERVICES_DIALOG).show();
			} else {
				Toast.makeText(getApplicationContext(),
						"Google Play Service is not installed",
						Toast.LENGTH_SHORT).show();
				finish();
			}
			return false;
		}
		return true;

	}

	private SharedPreferences getSharedPreferences() {
		if (prefs == null) {
			prefs = getApplicationContext().getSharedPreferences(
					"AndroidSRCDemo", Context.MODE_PRIVATE);
		}
		return prefs;
	}

	private void saveInSharedPref(String result) {
		// TODO Auto-generated method stub
		SharedPreferences.Editor editor = getSharedPreferences().edit();
		editor.putString(PREF_GCM_REG_ID, result);
		editor.apply();
	}

	private void deleteFromSharedPref() {
		// TODO Auto-generated method stub
		SharedPreferences.Editor editor = getSharedPreferences().edit();
		editor.remove(PREF_GCM_REG_ID);
		editor.apply();
	}

	@SuppressLint ("HandlerLeak")
	private final Handler handler = new Handler() {
		public void handleMessage(android.os.Message msg) {
			switch (msg.what) {
				case MSG_REGISTER_WITH_GCM:
					new GCMRegistrationTask().execute();
					break;
				case MSG_REGISTER_WEB_SERVER:
					new WebServerTask().execute("/?regid=" + gcmRegId);
					break;
				case MSG_REGISTER_WEB_SERVER_SUCCESS:
					Toast.makeText(getApplicationContext(),
							"registered with web server", Toast.LENGTH_LONG).show();
					showHideButtons(true);
					break;
				case MSG_REGISTER_WEB_SERVER_FAILURE:
					Toast.makeText(getApplicationContext(),
							"registration with web server failed",
							Toast.LENGTH_LONG).show();
					break;
			}
		}
	};

	private class GCMRegistrationTask extends AsyncTask<Void, Void, String> {

		@Override
		protected String doInBackground(Void... params) {
			// TODO Auto-generated method stub
			if (gcm == null && isGooglePlayInstalled()) {
				gcm = GoogleCloudMessaging.getInstance(getApplicationContext());
			}
			try {
				if (gcm != null) {
					gcmRegId = gcm.register(GCM_SENDER_ID);
				}
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}

			return gcmRegId;
		}

		@Override
		protected void onPostExecute(String result) {
			if (result != null) {
				Toast.makeText(getApplicationContext(), "registered with GCM",
						Toast.LENGTH_LONG).show();
				regIdView.setText(result);
				saveInSharedPref(result);
				handler.sendEmptyMessage(MSG_REGISTER_WEB_SERVER);
			}
		}
	}

	private class WebServerTask extends AsyncTask<String, Void, JSONObject> {

		@Override
		protected JSONObject doInBackground(String... params) {
			URL url = null;
			// Build url to register GCM ID
			String espURL = WEB_SERVER_URL + params[0];
			try {
				url = new URL(espURL);
			} catch (MalformedURLException e) {
				e.printStackTrace();
				handler.sendEmptyMessage(MSG_REGISTER_WEB_SERVER_FAILURE);
			}
			JSONObject jsonResult = null;

			HttpURLConnection conn = null;
			try {
				assert url != null;
				conn = (HttpURLConnection) url.openConnection();
				conn.setDoOutput(true);
				conn.setUseCaches(false);
				conn.setRequestMethod("GET");
				conn.setRequestProperty("Content-Type",
						"application/x-www-form-urlencoded;charset=UTF-8");

				int status = conn.getResponseCode();
				String response = conn.getResponseMessage();
				if (status == 200) {
					// Request success
					if (params[0].startsWith("/?regid=")) {
						handler.sendEmptyMessage(MSG_REGISTER_WEB_SERVER_SUCCESS);
					}
					response = readStream(conn.getInputStream());
				} else {
					if (BuildConfig.DEBUG)
						Log.d(DEBUG_LOG_TAG, "Request failed with error code " + status);
					//throw new IOException("Request failed with error code "	+ status);
				}
				try {
					jsonResult = new JSONObject(response);
				} catch (JSONException e) {
					if (BuildConfig.DEBUG)
						Log.d(DEBUG_LOG_TAG, "Create JSONObject from String failed " + e.getMessage());
					jsonResult = null;
				}
			} catch (IOException io) {
				io.printStackTrace();
				handler.sendEmptyMessage(MSG_REGISTER_WEB_SERVER_FAILURE);
			} finally {
				if (conn != null) {
					conn.disconnect();
				}
			}
			return jsonResult;
		}

		protected void onPostExecute(JSONObject result) {
			activityUpdate(result);
		}
	}

	private String readStream(InputStream in) {
		BufferedReader reader = null;
		StringBuilder response = new StringBuilder();
		try {
			reader = new BufferedReader(new InputStreamReader(in));
			String line;
			while ((line = reader.readLine()) != null) {
				response.append(line);
			}
		} catch (IOException e) {
			e.printStackTrace();
		} finally {
			if (reader != null) {
				try {
					reader.close();
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
		}
		return response.toString();
	}

	/**
	 * Update UI with values received from ESP device
	 *
	 * @param result
	 * 		result sent by onPostExecute
	 */
	private void activityUpdate(final JSONObject result) {
		runOnUiThread(new Runnable() {
			@SuppressWarnings("deprecation")
			@Override
			public void run() {
				String jsonValue = "";
				String uiText;
				if (result != null) {
					uiText = "Result: ";
					try {
						jsonValue = result.getString("result");
						uiText += jsonValue + "\n";
					} catch (JSONException e) {
						if (BuildConfig.DEBUG)
							Log.d(DEBUG_LOG_TAG, "Missing result in JSON object" + e.getMessage());
						uiText += "missing\n";
					}
					if (jsonValue.equalsIgnoreCase("success")) {
						if (result.has("num")) { // We got a list of registered devices
							uiText += "List of registered devices:\n";
							int numDevices = 0;
							try {
								numDevices = result.getInt("num");
							} catch (JSONException e) {
								if (BuildConfig.DEBUG)
									Log.d(DEBUG_LOG_TAG, "Missing num in JSON object" + e.getMessage());
							}
							for (int i=0; i<numDevices; i++) {
								try {
									jsonValue = result.getString(Integer.toString(i));
									uiText += "Device index " + Integer.toString(i) + " :\n" + jsonValue + "\n";
								} catch (JSONException e) {
									if (BuildConfig.DEBUG)
										Log.d(DEBUG_LOG_TAG, "Missing device reg id in JSON object" + e.getMessage());
								}
							}
						}
					} else {
						try {
							jsonValue = result.getString("reason");
							uiText += "Reason: " + jsonValue + "\n";
						} catch (JSONException e) {
							if (BuildConfig.DEBUG)
								Log.d(DEBUG_LOG_TAG, "Missing reason in JSON object" + e.getMessage());
							uiText += "unknown\n";
						}
					}

				} else {
					uiText = "No response";
				}
				regIdView.setText(uiText);
			}
		});
	}

	private void showHideButtons(boolean isRegistered) {
		if (isRegistered) {
			registerBtn.setVisibility(View.GONE);
			unRegisterBtn.setVisibility(View.VISIBLE);
			listBtn.setVisibility(View.VISIBLE);
		} else {
			registerBtn.setVisibility(View.VISIBLE);
			unRegisterBtn.setVisibility(View.GONE);
			listBtn.setVisibility(View.GONE);
		}
	}

	class updateUI implements Runnable {
		private final String msg;

		public updateUI(String str) {
			this.msg = str;
		}

		@Override
		public void run() {
			regIdView.setText(msg);
		}
	}

	private final BroadcastReceiver activityReceiver = new BroadcastReceiver() {

		@Override
		public void onReceive(Context context, Intent intent) {
			String message = "Received from " +
					intent.getStringExtra("sender") +
					" the message\n" +
					intent.getStringExtra("message");

			updateUIHandler.post(new updateUI(message));
		}
	};
}
