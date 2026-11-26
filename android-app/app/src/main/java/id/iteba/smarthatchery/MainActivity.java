package id.iteba.smarthatchery;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.webkit.CookieManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.Toast;

import com.google.firebase.messaging.FirebaseMessaging;

public class MainActivity extends Activity {

    private static final String PREFS_NAME = "smart_hatchery_prefs";
    private static final String KEY_SERVER_URL = "server_url";
    private static final String DEFAULT_URL = "https://app.itebaincubatortelur.cc.cd";
    private static final String CHANNEL_ID = "smart_hatchery_alerts";
    private static final int REQ_POST_NOTIFICATIONS = 1001;

    private WebView webView;
    private int notifCounter = 100;

    // JS Shim: Menangkap pemanggilan `new Notification(...)` dari frontend Svelte
    // dan meneruskannya ke notifikasi asli Android (berbunyi & muncul di status bar).
    private static final String NOTIFICATION_BRIDGE_JS =
            "(function(){"
            + "if(window.__androidNotifBridged) return;"
            + "window.__androidNotifBridged = true;"
            + "function NativeNotification(title, options){"
            + "  var body = (options && options.body) ? String(options.body) : '';"
            + "  var tag = (options && options.tag) ? String(options.tag) : '';"
            + "  if(window.AndroidBridge){ window.AndroidBridge.showNotification(String(title || 'Smart Hatchery'), body, tag); }"
            + "}"
            + "NativeNotification.permission = 'granted';"
            + "NativeNotification.requestPermission = function(cb){"
            + "  if(typeof cb === 'function') cb('granted');"
            + "  return Promise.resolve('granted');"
            + "};"
            + "window.Notification = NativeNotification;"
            + "})();";

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        createNotificationChannel();
        requestNotificationPermissionIfNeeded();
        FirebaseMessaging.getInstance().subscribeToTopic(HatcheryMessagingService.TOPIC_ALERTS);

        webView = new WebView(this);
        setContentView(webView);

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        settings.setBuiltInZoomControls(false);
        settings.setDisplayZoomControls(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE);

        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.setAcceptCookie(true);
        cookieManager.setAcceptThirdPartyCookies(webView, true);

        webView.addJavascriptInterface(new AndroidBridge(), "AndroidBridge");
        webView.setWebChromeClient(new WebChromeClient());
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                super.onPageStarted(view, url, favicon);
                view.evaluateJavascript(NOTIFICATION_BRIDGE_JS, null);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                view.evaluateJavascript(NOTIFICATION_BRIDGE_JS, null);
                CookieManager.getInstance().flush();
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request, WebResourceError error) {
                if (request != null && request.isForMainFrame()) {
                    showOfflinePage();
                }
            }
        });

        webView.loadUrl(getServerUrl());
    }

    private String getServerUrl() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getString(KEY_SERVER_URL, DEFAULT_URL);
    }

    private void saveServerUrl(String url) {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_SERVER_URL, url).apply();
    }

    private void showOfflinePage() {
        String currentUrl = getServerUrl();
        String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                + "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                + "<style>body{font-family:sans-serif;background:#f1f5f9;color:#0f172a;display:flex;align-items:center;justify-content:center;min-height:90vh;margin:0;padding:24px;}"
                + ".card{background:#fff;border:1px solid #cbd5e1;border-radius:12px;padding:24px;max-width:360px;width:100%;text-align:center;}"
                + "h2{margin:0 0 8px;font-size:20px;}p{font-size:14px;color:#475569;margin:0 0 18px;word-break:break-all;}"
                + "button{width:100%;padding:12px;border-radius:8px;border:none;font-size:15px;font-weight:600;margin-top:8px;cursor:pointer;}"
                + ".primary{background:#0f172a;color:#fff;}.secondary{background:#e2e8f0;color:#0f172a;}</style></head>"
                + "<body><div class='card'><h2>Tidak Terhubung ke Server</h2>"
                + "<p>Gagal memuat:<br><b>" + currentUrl + "</b><br><br>Periksa koneksi internet HP Anda atau status VPS.</p>"
                + "<button class='primary' onclick='AndroidBridge.reloadServer()'>Coba Lagi</button>"
                + "<button class='secondary' onclick='AndroidBridge.openUrlDialog()'>Ganti Alamat Server</button>"
                + "</div></body></html>";
        webView.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
    }

    private void showChangeUrlDialog() {
        runOnUiThread(() -> {
            final EditText input = new EditText(this);
            input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
            input.setText(getServerUrl());
            input.setSelection(input.getText().length());

            new AlertDialog.Builder(this)
                    .setTitle("Alamat Server Smart Hatchery")
                    .setMessage("Masukkan URL VPS (contoh: https://itebaincubatortelur.cc.cd):")
                    .setView(input)
                    .setPositiveButton("Simpan & Muat", (dialog, which) -> {
                        String newUrl = input.getText().toString().trim();
                        if (!newUrl.startsWith("http://") && !newUrl.startsWith("https://")) {
                            newUrl = "https://" + newUrl;
                        }
                        saveServerUrl(newUrl);
                        webView.loadUrl(newUrl);
                    })
                    .setNegativeButton("Batal", null)
                    .show();
        });
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "Peringatan Inkubator Telur",
                    NotificationManager.IMPORTANCE_HIGH
            );
            channel.setDescription("Notifikasi suhu kritis, gangguan motor, dan penyelesaian siklus inkubasi 21 hari.");
            channel.enableVibration(true);
            channel.setVibrationPattern(new long[]{0, 400, 200, 400});
            NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    private void requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= 33) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_POST_NOTIFICATIONS);
            }
        }
    }

    private void fireAndroidNotification(String title, String body, String tag) {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) return;

        Intent intent = new Intent(this, MainActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        int piFlags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            piFlags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, piFlags);

        Uri soundUri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
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

        int id = (tag != null && !tag.isEmpty()) ? tag.hashCode() : (++notifCounter);
        manager.notify(id, notif);
    }

    public class AndroidBridge {
        @JavascriptInterface
        public void showNotification(String title, String body, String tag) {
            runOnUiThread(() -> fireAndroidNotification(title, body, tag));
        }

        @JavascriptInterface
        public void reloadServer() {
            runOnUiThread(() -> webView.loadUrl(getServerUrl()));
        }

        @JavascriptInterface
        public void openUrlDialog() {
            showChangeUrlDialog();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        CookieManager.getInstance().flush();
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }
}
