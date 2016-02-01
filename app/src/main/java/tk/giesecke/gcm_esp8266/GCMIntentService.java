package tk.giesecke.gcm_esp8266;

import android.app.IntentService;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;


/**
 * An {@link IntentService} subclass for handling asynchronous task requests in
 * a service on a separate handler thread.
 * <p/>
 * TODO: Customize class - update intent actions and extra parameters.
 */
public class GCMIntentService extends IntentService {

	private static final int NOTIFICATION_ID = 1000;
	public static final String BROADCAST_RECEIVED = "BC_RECEIVED";

	public GCMIntentService() {
		super(GCMIntentService.class.getName());
	}

	@Override
	protected void onHandleIntent(Intent intent) {
		Bundle extras = intent.getExtras();

		if (!extras.isEmpty()) {

			// read extras as sent from server
			String message = extras.getString("message");
			String serverTime = extras.getString("timestamp");
			String displayTxt = "Message: " + message + "\n" + "Server Time: " + serverTime;
			sendNotification(displayTxt);
			sendGCMBroadcast(displayTxt);
		}
		// Release the wake lock provided by the WakefulBroadcastReceiver.
		GCMBroadcastReceiver.completeWakefulIntent(intent);
	}

	private void sendNotification(String msg) {
		NotificationManager mNotificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

		PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
				new Intent(this, MainActivity.class), 0);

		NotificationCompat.Builder mBuilder = new NotificationCompat.Builder(this)
				.setSmallIcon(R.mipmap.ic_launcher)
				.setContentTitle("Notification from GCM")
				.setStyle(new NotificationCompat.BigTextStyle().bigText(msg))
				.setContentText(msg);

		mBuilder.setContentIntent(contentIntent);
		mNotificationManager.notify(NOTIFICATION_ID, mBuilder.build());
	}

	//send broadcast from activity to all receivers listening to the action "ACTION_STRING_ACTIVITY"
	private void sendGCMBroadcast(String msgReceived) {
		Intent broadCastIntent = new Intent();
		broadCastIntent.setAction(BROADCAST_RECEIVED);
		broadCastIntent.putExtra("sender", "GCM");
		broadCastIntent.putExtra("message", msgReceived);
		sendBroadcast(broadCastIntent);
	}
}
