package id.iteba.smarthatchery;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import java.util.Map;

public class HatcheryMessagingService extends FirebaseMessagingService {

    public static final String CHANNEL_ID = "smart_hatchery_alerts";
    public static final String TOPIC_ALERTS = "incubator_alerts";

    @Override
    public void onNewToken(String token) {
        super.onNewToken(token);
        FirebaseMessaging.getInstance().subscribeToTopic(TOPIC_ALERTS);
    }

    @Override
    public void onMessageReceived(RemoteMessage remoteMessage) {
        super.onMessageReceived(remoteMessage);

        String title = "Smart Hatchery";
        String body = "";

        if (remoteMessage.getNotification() != null) {
            if (remoteMessage.getNotification().getTitle() != null) {
                title = remoteMessage.getNotification().getTitle();
            }
            if (remoteMessage.getNotification().getBody() != null) {
                body = remoteMessage.getNotification().getBody();
            }
        }

        Map<String, String> data = remoteMessage.getData();
        if (data != null && !data.isEmpty()) {
            if (body.isEmpty() && data.get("body") != null) {
                body = data.get("body");
            }
            if ("Smart Hatchery".equals(title) && data.get("title") != null) {
                title = data.get("title");
            }
        }

        if (!body.isEmpty()) {
            showPushNotification(this, title, body, data != null ? data.get("alert_type") : null);
        }
    }

    public static void ensureChannel(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "Peringatan Inkubator Telur",
                    NotificationManager.IMPORTANCE_HIGH
            );
            channel.setDescription("Notifikasi darurat suhu, motor, koneksi ESP32, dan siklus inkubasi 21 hari.");
            channel.enableVibration(true);
            channel.setVibrationPattern(new long[]{0, 400, 200, 400});
            NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    public static void showPushNotification(Context context, String title, String body, String tag) {
        ensureChannel(context);
        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) return;

        Intent intent = new Intent(context, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        int piFlags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            piFlags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, intent, piFlags);

        Uri soundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(context, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(context);
            builder.setPriority(Notification.PRIORITY_HIGH);
            builder.setSound(soundUri);
        }

        Notification notif = builder
                .setSmallIcon(R.drawable.ic_notif)
                .setContentTitle(title != null && !title.isEmpty() ? title : "Smart Hatchery")
                .setContentText(body != null ? body : "")
                .setStyle(new Notification.BigTextStyle().bigText(body != null ? body : ""))
                .setAutoCancel(true)
                .setContentIntent(pendingIntent)
                .build();

        int id = (tag != null && !tag.isEmpty()) ? tag.hashCode() : (int) (System.currentTimeMillis() & 0x7FFFFFFF);
        Log.d("SmartHatcheryFCM", "Showing notification: " + title);
        manager.notify(id, notif);
    }
}
