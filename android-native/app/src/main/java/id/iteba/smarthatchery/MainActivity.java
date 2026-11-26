package id.iteba.smarthatchery;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.firebase.messaging.FirebaseMessaging;

import org.json.JSONArray;
import org.json.JSONObject;

import java.time.Instant;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {

    private static final int COL_BG = Color.parseColor("#0B1120");
    private static final int COL_PANEL = Color.parseColor("#131C31");
    private static final int COL_CARD = Color.parseColor("#19243D");
    private static final int COL_LINE = Color.parseColor("#2A3958");
    private static final int COL_TEXT = Color.parseColor("#F8FAFC");
    private static final int COL_MUTED = Color.parseColor("#94A3B8");
    private static final int COL_AMBER = Color.parseColor("#F59E0B");
    private static final int COL_GREEN = Color.parseColor("#10B981");
    private static final int COL_RED = Color.parseColor("#EF4444");

    private ApiClient api;
    private final ExecutorService executor = Executors.newCachedThreadPool();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private FrameLayout rootContainer;
    private LinearLayout tabContentContainer;
    private TextView headerStatusBadge;
    private final TextView[] navLabels = new TextView[5];
    private final View[] navIndicators = new View[5];

    private JSONObject currentUser = null;
    private JSONObject telemetry = null;
    private JSONArray alerts = new JSONArray();
    private JSONArray auditLogs = new JSONArray();
    private JSONObject deviceSettings = null;
    private String lastTelemetryError = "";

    private int activeTab = 0; // 0=Beranda, 1=Kontrol, 2=Peringatan, 3=Atur, 4=Riwayat
    private String alertFilter = "semua"; // semua | active | resolved

    // Draft state for Kontrol & Pengaturan so 5s polling doesn't overwrite user typing/sliders
    private boolean kontrolInitialized = false;
    private String draftLampMode = "auto";
    private int draftLampVal = 50;
    private String draftVentMode = "auto";
    private int draftVentVal = 30;

    private boolean settingsLoaded = false;
    private EditText inputTargetTemp;
    private EditText inputMotorInterval;
    private EditText inputMotorDuration;
    private EditText inputMotorSpeed;
    private EditText inputWifiSsid;
    private EditText inputWifiPass;

    private boolean pollingActive = false;
    private Set<Long> seenActiveAlerts = null;

    private final Runnable telemetryRunnable = new Runnable() {
        @Override
        public void run() {
            if (!pollingActive || currentUser == null) return;
            fetchTelemetryNow();
            mainHandler.postDelayed(this, 5000);
        }
    };

    private final Runnable alertsRunnable = new Runnable() {
        @Override
        public void run() {
            if (!pollingActive || currentUser == null) return;
            fetchAlertsNow();
            mainHandler.postDelayed(this, 10000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        api = new ApiClient(this);
        HatcheryMessagingService.ensureChannel(this);
        requestNotificationPermission();
        FirebaseMessaging.getInstance().subscribeToTopic(HatcheryMessagingService.TOPIC_ALERTS);

        rootContainer = new FrameLayout(this);
        rootContainer.setBackgroundColor(COL_BG);
        setContentView(rootContainer);

        showLoadingScreen("Memeriksa sesi pengguna…");
        checkSessionOnBoot();
    }

    private void requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= 33) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 2001);
            }
        }
    }

    private int dp(int val) {
        return Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, val, getResources().getDisplayMetrics()));
    }

    private GradientDrawable roundedBox(int fillColor, int strokeColor, int radiusDp) {
        GradientDrawable gd = new GradientDrawable();
        gd.setColor(fillColor);
        gd.setCornerRadius(dp(radiusDp));
        if (strokeColor != 0) {
            gd.setStroke(dp(1), strokeColor);
        }
        return gd;
    }

    private void showLoadingScreen(String msg) {
        rootContainer.removeAllViews();
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        TextView tv = new TextView(this);
        tv.setText(msg);
        tv.setTextColor(COL_MUTED);
        tv.setTextSize(15);
        box.addView(tv);
        rootContainer.addView(box, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    private void checkSessionOnBoot() {
        executor.execute(() -> {
            try {
                JSONObject res = api.getJson("/auth/me");
                JSONObject u = res.optJSONObject("user");
                mainHandler.post(() -> {
                    if (u != null) {
                        currentUser = u;
                        enterAuthenticatedShell();
                    } else {
                        showLoginScreen();
                    }
                });
            } catch (Exception e) {
                mainHandler.post(this::showLoginScreen);
            }
        });
    }

    // =========================================================================
    // LOGIN SCREEN (100% NATIVE)
    // =========================================================================
    private void showLoginScreen() {
        stopPolling();
        rootContainer.removeAllViews();

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);

        LinearLayout outer = new LinearLayout(this);
        outer.setOrientation(LinearLayout.VERTICAL);
        outer.setGravity(Gravity.CENTER);
        outer.setPadding(dp(24), dp(36), dp(24), dp(36));

        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(roundedBox(COL_PANEL, COL_LINE, 18));
        card.setPadding(dp(22), dp(26), dp(22), dp(26));

        TextView badge = new TextView(this);
        badge.setText("NATIVE ANDROID · FCM ACTIVE");
        badge.setTextSize(11);
        badge.setTypeface(Typeface.DEFAULT_BOLD);
        badge.setTextColor(COL_AMBER);
        card.addView(badge);

        TextView title = new TextView(this);
        title.setText("Smart Hatchery");
        title.setTextSize(24);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextColor(COL_TEXT);
        title.setPadding(0, dp(4), 0, dp(4));
        card.addView(title);

        TextView sub = new TextView(this);
        sub.setText("Masuk sebagai Operator untuk memantau & mengendalikan inkubator telur 21 hari.");
        sub.setTextSize(13);
        sub.setTextColor(COL_MUTED);
        sub.setPadding(0, 0, 0, dp(18));
        card.addView(sub);

        card.addView(makeFieldLabel("Nama Pengguna"));
        EditText userField = makeEditText("operator1", InputType.TYPE_CLASS_TEXT);
        card.addView(userField);

        View spacer1 = new View(this);
        spacer1.setLayoutParams(new LinearLayout.LayoutParams(1, dp(12)));
        card.addView(spacer1);

        card.addView(makeFieldLabel("Kata Sandi"));
        EditText passField = makeEditText("••••••••", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        card.addView(passField);

        TextView errView = new TextView(this);
        errView.setTextColor(COL_RED);
        errView.setTextSize(13);
        errView.setPadding(0, dp(12), 0, 0);
        errView.setVisibility(View.GONE);
        card.addView(errView);

        Button loginBtn = makePrimaryButton("Masuk", COL_AMBER, Color.parseColor("#0F172A"));
        LinearLayout.LayoutParams btnLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        btnLp.topMargin = dp(18);
        loginBtn.setLayoutParams(btnLp);

        loginBtn.setOnClickListener(v -> {
            String u = userField.getText().toString().trim();
            String p = passField.getText().toString();
            if (u.isEmpty() || p.isEmpty()) {
                errView.setText("⚠ Isi nama pengguna dan kata sandi.");
                errView.setVisibility(View.VISIBLE);
                return;
            }
            errView.setVisibility(View.GONE);
            loginBtn.setEnabled(false);
            loginBtn.setText("Memeriksa…");

            executor.execute(() -> {
                try {
                    JSONObject body = new JSONObject();
                    body.put("username", u);
                    body.put("password", p);
                    JSONObject res = api.postJson("/auth/login", body);
                    JSONObject userObj = res.optJSONObject("user");
                    mainHandler.post(() -> {
                        currentUser = userObj != null ? userObj : new JSONObject();
                        enterAuthenticatedShell();
                    });
                } catch (Exception ex) {
                    mainHandler.post(() -> {
                        loginBtn.setEnabled(true);
                        loginBtn.setText("Masuk");
                        errView.setText("⚠ " + ex.getMessage());
                        errView.setVisibility(View.VISIBLE);
                    });
                }
            });
        });
        card.addView(loginBtn);

        outer.addView(card, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        scroll.addView(outer);
        rootContainer.addView(scroll);
    }

    // =========================================================================
    // AUTHENTICATED SHELL (TOP HEADER + CONTENT + 5-ITEM BOTTOM NAVBAR)
    // =========================================================================
    private void enterAuthenticatedShell() {
        rootContainer.removeAllViews();
        kontrolInitialized = false;
        settingsLoaded = false;

        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(LinearLayout.VERTICAL);

        // Top App Header
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setBackgroundColor(COL_PANEL);
        header.setPadding(dp(16), dp(12), dp(16), dp(12));

        LinearLayout titleCol = new LinearLayout(this);
        titleCol.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams titleLp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        titleCol.setLayoutParams(titleLp);

        TextView appTitle = new TextView(this);
        appTitle.setText("Smart Hatchery");
        appTitle.setTextSize(17);
        appTitle.setTypeface(Typeface.DEFAULT_BOLD);
        appTitle.setTextColor(COL_TEXT);
        titleCol.addView(appTitle);

        TextView subTitle = new TextView(this);
        String uname = currentUser != null ? currentUser.optString("username", "operator") : "operator";
        subTitle.setText("Native App · " + uname);
        subTitle.setTextSize(11);
        subTitle.setTextColor(COL_MUTED);
        titleCol.addView(subTitle);
        header.addView(titleCol);

        headerStatusBadge = new TextView(this);
        headerStatusBadge.setText("Memuat…");
        headerStatusBadge.setTextSize(11);
        headerStatusBadge.setTypeface(Typeface.DEFAULT_BOLD);
        headerStatusBadge.setTextColor(COL_AMBER);
        headerStatusBadge.setBackground(roundedBox(Color.parseColor("#292524"), COL_AMBER, 99));
        headerStatusBadge.setPadding(dp(10), dp(4), dp(10), dp(4));
        header.addView(headerStatusBadge);

        Button logoutBtn = new Button(this);
        logoutBtn.setText("Keluar");
        logoutBtn.setAllCaps(false);
        logoutBtn.setTextSize(12);
        logoutBtn.setTextColor(COL_TEXT);
        logoutBtn.setBackground(roundedBox(COL_CARD, COL_LINE, 10));
        logoutBtn.setPadding(dp(12), dp(4), dp(12), dp(4));
        LinearLayout.LayoutParams outLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
        outLp.leftMargin = dp(8);
        logoutBtn.setLayoutParams(outLp);
        logoutBtn.setOnClickListener(v -> performLogout());
        header.addView(logoutBtn);

        shell.addView(header, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        View dividerTop = new View(this);
        dividerTop.setBackgroundColor(COL_LINE);
        shell.addView(dividerTop, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)));

        // Scrollable Content Area
        ScrollView contentScroll = new ScrollView(this);
        contentScroll.setFillViewport(true);
        LinearLayout.LayoutParams scrollLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f);
        contentScroll.setLayoutParams(scrollLp);

        tabContentContainer = new LinearLayout(this);
        tabContentContainer.setOrientation(LinearLayout.VERTICAL);
        tabContentContainer.setPadding(dp(14), dp(14), dp(14), dp(22));
        contentScroll.addView(tabContentContainer);
        shell.addView(contentScroll);

        // Bottom Divider & 5-Tab Navbar
        View dividerBot = new View(this);
        dividerBot.setBackgroundColor(COL_LINE);
        shell.addView(dividerBot, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)));

        LinearLayout navBar = new LinearLayout(this);
        navBar.setOrientation(LinearLayout.HORIZONTAL);
        navBar.setBackgroundColor(COL_PANEL);
        navBar.setPadding(dp(4), dp(6), dp(4), dp(8));

        String[] tabNames = {"Beranda", "Kontrol", "Peringatan", "Atur", "Riwayat"};
        for (int i = 0; i < 5; i++) {
            final int idx = i;
            LinearLayout tabItem = new LinearLayout(this);
            tabItem.setOrientation(LinearLayout.VERTICAL);
            tabItem.setGravity(Gravity.CENTER);
            tabItem.setPadding(dp(4), dp(4), dp(4), dp(4));
            tabItem.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

            View pill = new View(this);
            pill.setBackground(roundedBox(COL_AMBER, 0, 99));
            LinearLayout.LayoutParams pillLp = new LinearLayout.LayoutParams(dp(28), dp(3));
            pillLp.bottomMargin = dp(4);
            pill.setLayoutParams(pillLp);
            navIndicators[i] = pill;
            tabItem.addView(pill);

            TextView lbl = new TextView(this);
            lbl.setText(tabNames[i]);
            lbl.setTextSize(12);
            lbl.setGravity(Gravity.CENTER);
            navLabels[i] = lbl;
            tabItem.addView(lbl);

            tabItem.setOnClickListener(v -> switchTab(idx));
            navBar.addView(tabItem);
        }

        shell.addView(navBar, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        rootContainer.addView(shell, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        startPolling();
        switchTab(activeTab);
    }

    private void switchTab(int idx) {
        activeTab = idx;
        for (int i = 0; i < 5; i++) {
            boolean sel = (i == idx);
            navIndicators[i].setVisibility(sel ? View.VISIBLE : View.INVISIBLE);
            navLabels[i].setTextColor(sel ? COL_AMBER : COL_MUTED);
            navLabels[i].setTypeface(sel ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
        }
        if (idx == 3 && !settingsLoaded) {
            fetchDeviceSettings();
        } else if (idx == 4) {
            fetchAuditLogs();
        }
        renderActiveTab();
    }

    private void performLogout() {
        stopPolling();
        executor.execute(() -> {
            try {
                api.postJson("/auth/logout", new JSONObject());
            } catch (Exception ignored) {}
            api.clearSession();
            mainHandler.post(() -> {
                currentUser = null;
                telemetry = null;
                showLoginScreen();
            });
        });
    }

    // =========================================================================
    // POLLING & DATA FETCHERS
    // =========================================================================
    private void startPolling() {
        if (pollingActive) return;
        pollingActive = true;
        seenActiveAlerts = null;
        mainHandler.post(telemetryRunnable);
        mainHandler.post(alertsRunnable);
    }

    private void stopPolling() {
        pollingActive = false;
        mainHandler.removeCallbacks(telemetryRunnable);
        mainHandler.removeCallbacks(alertsRunnable);
    }

    private void fetchTelemetryNow() {
        executor.execute(() -> {
            try {
                JSONObject d = api.getJson("/telemetry/latest");
                mainHandler.post(() -> {
                    telemetry = d;
                    lastTelemetryError = "";
                    if (!kontrolInitialized && d != null) {
                        JSONObject lamp = d.optJSONObject("lamp");
                        JSONObject vent = d.optJSONObject("ventilation");
                        if (lamp != null) {
                            draftLampMode = lamp.optString("mode", "auto");
                            draftLampVal = Math.min(lamp.optInt("intensity", 50), 80);
                        }
                        if (vent != null) {
                            draftVentMode = vent.optString("mode", "auto");
                            draftVentVal = Math.min(vent.optInt("angle", 30), 90);
                        }
                        kontrolInitialized = true;
                    }
                    updateHeaderBadge();
                    if (activeTab == 0 || activeTab == 1) {
                        renderActiveTab();
                    }
                });
            } catch (ApiClient.ApiException ae) {
                mainHandler.post(() -> {
                    if (ae.statusCode == 401) {
                        currentUser = null;
                        showLoginScreen();
                    } else {
                        lastTelemetryError = ae.getMessage();
                        updateHeaderBadge();
                        if (activeTab == 0) renderActiveTab();
                    }
                });
            } catch (Exception e) {
                mainHandler.post(() -> {
                    lastTelemetryError = "Gagal terhubung ke server";
                    updateHeaderBadge();
                    if (activeTab == 0) renderActiveTab();
                });
            }
        });
    }

    private void fetchAlertsNow() {
        executor.execute(() -> {
            try {
                JSONArray arr = api.getJsonArray("/alerts");
                mainHandler.post(() -> {
                    alerts = arr;
                    notifyNewAlertsIfAny(arr);
                    if (activeTab == 0 || activeTab == 2) {
                        renderActiveTab();
                    }
                });
            } catch (Exception ignored) {}
        });
    }

    private void notifyNewAlertsIfAny(JSONArray arr) {
        if (arr == null) return;
        if (seenActiveAlerts == null) {
            seenActiveAlerts = new HashSet<>();
            for (int i = 0; i < arr.length(); i++) {
                JSONObject a = arr.optJSONObject(i);
                if (a != null && "active".equals(a.optString("status"))) {
                    seenActiveAlerts.add(a.optLong("id"));
                }
            }
            return;
        }
        for (int i = 0; i < arr.length(); i++) {
            JSONObject a = arr.optJSONObject(i);
            if (a != null && "active".equals(a.optString("status"))) {
                long id = a.optLong("id");
                if (!seenActiveAlerts.contains(id)) {
                    seenActiveAlerts.add(id);
                    String sev = a.optString("severity", "medium");
                    String title = "high".equals(sev) ? "🚨 Peringatan Darurat Inkubator" : "⚠ Peringatan Inkubator";
                    HatcheryMessagingService.showPushNotification(
                            this, title, a.optString("message", ""), "alert-" + id);
                }
            }
        }
    }

    private void fetchDeviceSettings() {
        executor.execute(() -> {
            try {
                JSONObject s = api.getJson("/settings/device");
                mainHandler.post(() -> {
                    deviceSettings = s;
                    settingsLoaded = true;
                    if (activeTab == 3) renderActiveTab();
                });
            } catch (Exception e) {
                mainHandler.post(() -> Toast.makeText(this, "Gagal memuat pengaturan: " + e.getMessage(), Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void fetchAuditLogs() {
        executor.execute(() -> {
            try {
                JSONArray logs = api.getJsonArray("/audit-logs?page=1&limit=30");
                mainHandler.post(() -> {
                    auditLogs = logs;
                    if (activeTab == 4) renderActiveTab();
                });
            } catch (Exception ignored) {}
        });
    }

    private long getTelemetryAgeSeconds() {
        if (telemetry == null) return 999;
        String updatedStr = telemetry.optString("updated_at", "");
        String serverStr = telemetry.optString("server_time", "");
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !updatedStr.isEmpty()) {
                long upd = Instant.parse(updatedStr).getEpochSecond();
                long ref = !serverStr.isEmpty() ? Instant.parse(serverStr).getEpochSecond() : (System.currentTimeMillis() / 1000L);
                return Math.max(0, ref - upd);
            }
        } catch (Exception ignored) {}
        return 0;
    }

    private boolean isStaleOrOffline() {
        if (telemetry == null) return true;
        if ("offline".equalsIgnoreCase(telemetry.optString("connection", "offline"))) return true;
        return getTelemetryAgeSeconds() > 15;
    }

    private void updateHeaderBadge() {
        if (headerStatusBadge == null) return;
        if (telemetry == null || isStaleOrOffline()) {
            headerStatusBadge.setText("● Offline / Stale");
            headerStatusBadge.setTextColor(COL_AMBER);
            headerStatusBadge.setBackground(roundedBox(Color.parseColor("#292524"), COL_AMBER, 99));
        } else {
            headerStatusBadge.setText("● Online");
            headerStatusBadge.setTextColor(COL_GREEN);
            headerStatusBadge.setBackground(roundedBox(Color.parseColor("#062E24"), COL_GREEN, 99));
        }
    }

    // =========================================================================
    // TAB RENDERING DISPATCHER
    // =========================================================================
    private void renderActiveTab() {
        if (tabContentContainer == null) return;
        // Avoid wiping typed text while user is editing Pengaturan fields if already rendered
        if (activeTab == 3 && settingsLoaded && inputTargetTemp != null && tabContentContainer.getChildCount() > 1) {
            return;
        }
        tabContentContainer.removeAllViews();

        switch (activeTab) {
            case 0:
                renderDashboardTab();
                break;
            case 1:
                renderKontrolTab();
                break;
            case 2:
                renderAlertsTab();
                break;
            case 3:
                renderSettingsTab();
                break;
            case 4:
                renderAuditTab();
                break;
        }
    }

    // =========================================================================
    // TAB 0: BERANDA (DASHBOARD)
    // =========================================================================
    private void renderDashboardTab() {
        if (!lastTelemetryError.isEmpty()) {
            tabContentContainer.addView(makeBanner("⚠ " + lastTelemetryError, COL_RED));
        }
        if (telemetry == null) {
            tabContentContainer.addView(makeSectionTitle("Memuat data telemetri dari VPS…"));
            return;
        }

        boolean stale = isStaleOrOffline();
        double temp = telemetry.optDouble("temperature", 0);
        double targetTemp = telemetry.optDouble("target_temperature", 37.5);
        double hum = telemetry.optDouble("humidity", 0);
        JSONObject inc = telemetry.optJSONObject("incubation");
        JSONObject dc = telemetry.optJSONObject("device_condition");
        JSONObject lamp = telemetry.optJSONObject("lamp");
        JSONObject vent = telemetry.optJSONObject("ventilation");
        JSONObject motor = telemetry.optJSONObject("motor");
        JSONObject turning = telemetry.optJSONObject("turning");
        JSONObject wifi = telemetry.optJSONObject("wifi");
        JSONObject lastCmd = telemetry.optJSONObject("last_command");

        if (stale) {
            tabContentContainer.addView(makeBanner(
                    "⏱ Data kedaluwarsa (" + getTelemetryAgeSeconds() + " dtk lalu) atau ESP32 sedang offline. Tombol kendali dikunci otomatis.",
                    COL_AMBER));
        }

        // Active Alerts Banner
        int activeCount = 0;
        StringBuilder alertLines = new StringBuilder();
        for (int i = 0; i < alerts.length(); i++) {
            JSONObject a = alerts.optJSONObject(i);
            if (a != null && "active".equals(a.optString("status"))) {
                activeCount++;
                if (alertLines.length() > 0) alertLines.append("\n");
                alertLines.append("• ").append(a.optString("message"));
            }
        }
        if (activeCount > 0) {
            tabContentContainer.addView(makeBanner(
                    "⚠ " + activeCount + " Peringatan Aktif:\n" + alertLines, COL_RED));
        }

        // 1. Proses Inkubasi 21 Hari Card
        LinearLayout incCard = makeCard();
        String incStatus = inc != null ? inc.optString("status", "idle") : "idle";
        String statusLabel = "running".equals(incStatus) ? "↻ Berjalan" : ("finished".equals(incStatus) ? "✓ Selesai" : "⏸ Belum Dimulai");
        int statusColor = "running".equals(incStatus) || "finished".equals(incStatus) ? COL_GREEN : COL_MUTED;

        incCard.addView(makeCardHeader("Proses Inkubasi (21 Hari)", statusLabel, statusColor));

        if ("running".equals(incStatus) && inc != null) {
            int day = inc.optInt("current_day", 1);
            int total = inc.optInt("total_days", 21);
            String stage = inc.optString("stage", "turning");
            String stageText = "lockdown".equals(stage) ? "Lockdown (Hari 19–21 · Motor Dikunci)" : "Fase Turning (Hari 1–18 · Putar Aktif)";
            TextView info = makeBodyText("Hari ke-" + day + " dari " + total + "  ·  " + stageText);
            info.setPadding(0, dp(8), 0, dp(10));
            incCard.addView(info);

            Button stopBtn = makePrimaryButton("Hentikan Siklus Inkubasi", COL_RED, Color.WHITE);
            stopBtn.setEnabled(!stale);
            stopBtn.setAlpha(stale ? 0.45f : 1f);
            stopBtn.setOnClickListener(v -> new AlertDialog.Builder(this)
                    .setTitle("Hentikan Inkubasi?")
                    .setMessage("Pemanasan dan pembalikan otomatis siklus 21 hari akan dihentikan.")
                    .setPositiveButton("Ya, Hentikan", (d, w) -> sendDeviceCommand("incubation", "stop", null, null, null))
                    .setNegativeButton("Batal", null)
                    .show());
            incCard.addView(stopBtn);
        } else {
            String desc = "finished".equals(incStatus)
                    ? "✓ Siklus 21 hari telah selesai! Telur memasuki masa menetas."
                    : "Inkubasi belum dimulai. Tekan tombol di bawah setelah telur dimasukkan.";
            TextView info = makeBodyText(desc);
            info.setPadding(0, dp(8), 0, dp(10));
            incCard.addView(info);

            Button startBtn = makePrimaryButton(
                    "finished".equals(incStatus) ? "Mulai Siklus Baru (21 Hari)" : "Mulai Inkubasi (21 Hari)",
                    COL_AMBER, Color.parseColor("#0F172A"));
            startBtn.setEnabled(!stale);
            startBtn.setAlpha(stale ? 0.45f : 1f);
            startBtn.setOnClickListener(v -> sendDeviceCommand("incubation", "start", null, null, null));
            incCard.addView(startBtn);
        }
        tabContentContainer.addView(incCard);

        // 2. Grid 6 Kartu Metrik (2x3)
        tabContentContainer.addView(makeSectionTitle("Kondisi Mikroklimat & Aktuator"));

        boolean tempOk = (temp >= 36.5 && temp <= 38.5);
        String lampMode = lamp != null && "auto".equals(lamp.optString("mode")) ? "Otomatis (Fuzzy)" : "Manual";
        String ventMode = vent != null && "auto".equals(vent.optString("mode")) ? "Otomatis" : "Manual";
        String turnSt = turning != null ? turning.optString("status", "idle") : "idle";
        boolean encOk = motor == null || motor.optBoolean("encoder_ok", true);
        boolean wifiConn = wifi != null && wifi.optBoolean("connected", false);

        tabContentContainer.addView(makeMetricRow(
                makeMetricTile("Suhu Aktual", String.format(Locale.US, "%.1f °C", temp),
                        String.format(Locale.US, "Target %.1f °C", targetTemp), tempOk ? COL_GREEN : COL_RED),
                makeMetricTile("Kelembapan", String.format(Locale.US, "%.0f %%", hum),
                        "Pengaman pasif", COL_TEXT)
        ));

        tabContentContainer.addView(makeMetricRow(
                makeMetricTile("Lampu Pemanas", (lamp != null ? lamp.optInt("intensity", 0) : 0) + " %",
                        lampMode, COL_AMBER),
                makeMetricTile("Ventilasi Servo", (vent != null ? vent.optInt("angle", 0) : 0) + "°",
                        ventMode, COL_TEXT)
        ));

        tabContentContainer.addView(makeMetricRow(
                makeMetricTile("Status Putar", formatTurnLabel(turnSt),
                        encOk ? "Encoder Normal" : "⚠ Encoder Fault", encOk ? COL_GREEN : COL_RED),
                makeMetricTile("Wi-Fi Alat", wifiConn ? wifi.optString("ssid", "-") : "Terputus",
                        wifiConn ? ("Sinyal " + wifi.optInt("rssi", 0) + " dBm") : "Offline", wifiConn ? COL_GREEN : COL_RED)
        ));

        // 3. Kondisi Operasional Alat & Status Perintah Terakhir
        if (dc != null) {
            LinearLayout dcCard = makeCard();
            String health = dc.optString("system_health", "normal");
            int hCol = "fault".equals(health) ? COL_RED : ("warning".equals(health) ? COL_AMBER : COL_GREEN);
            dcCard.addView(makeCardHeader("Kesehatan Sistem", health.toUpperCase(Locale.US), hCol));
            TextView sum = makeBodyText(dc.optString("summary", "-"));
            sum.setPadding(0, dp(6), 0, 0);
            dcCard.addView(sum);

            if (lastCmd != null) {
                String cmdSt = lastCmd.optString("status", "dispatched");
                TextView cmdTv = makeBodyText("Perintah terakhir (#" + lastCmd.optLong("id") + " · "
                        + lastCmd.optString("target") + "): " + cmdSt.toUpperCase(Locale.US));
                cmdTv.setTextColor("executed".equals(cmdSt) ? COL_GREEN : ("failed".equals(cmdSt) ? COL_RED : COL_AMBER));
                cmdTv.setPadding(0, dp(8), 0, 0);
                dcCard.addView(cmdTv);
            }
            tabContentContainer.addView(dcCard);
        }
    }

    // =========================================================================
    // TAB 1: KONTROL AKTUATOR
    // =========================================================================
    private void renderKontrolTab() {
        boolean stale = isStaleOrOffline();
        if (stale) {
            tabContentContainer.addView(makeBanner(
                    "⏱ Kontrol dinonaktifkan karena perangkat sedang offline atau data > 15 detik.", COL_AMBER));
        }

        // 1. Lampu Pemanas (0 - 80%)
        LinearLayout lampCard = makeCard();
        lampCard.addView(makeCardHeader("Lampu Pemanas (220V)",
                "auto".equals(draftLampMode) ? "Mode Fuzzy" : "Mode Manual", COL_AMBER));

        LinearLayout lampModeRow = makeModeSelectorRow(draftLampMode, mode -> {
            draftLampMode = mode;
            renderActiveTab();
        }, !stale);
        lampCard.addView(lampModeRow);

        if ("manual".equals(draftLampMode)) {
            TextView valLbl = makeBodyText("Intensitas Manual: " + draftLampVal + "% (Batas aman maks. 80%)");
            valLbl.setPadding(0, dp(10), 0, dp(4));
            lampCard.addView(valLbl);

            SeekBar sb = new SeekBar(this);
            sb.setMax(80);
            sb.setProgress(draftLampVal);
            sb.setEnabled(!stale);
            sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int p, boolean fromUser) {
                    draftLampVal = p;
                    valLbl.setText("Intensitas Manual: " + p + "% (Batas aman maks. 80%)");
                }
                @Override public void onStartTrackingTouch(SeekBar seekBar) {}
                @Override public void onStopTrackingTouch(SeekBar seekBar) {}
            });
            lampCard.addView(sb);
        } else {
            TextView note = makeBodyText("Intensitas diatur otomatis oleh inferensi Logika Fuzzy Mamdani di ESP32.");
            note.setPadding(0, dp(8), 0, dp(4));
            lampCard.addView(note);
        }

        Button applyLamp = makePrimaryButton("Terapkan Lampu", COL_AMBER, Color.parseColor("#0F172A"));
        applyLamp.setEnabled(!stale);
        applyLamp.setAlpha(stale ? 0.45f : 1f);
        LinearLayout.LayoutParams lp1 = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp1.topMargin = dp(10);
        applyLamp.setLayoutParams(lp1);
        applyLamp.setOnClickListener(v -> sendDeviceCommand(
                "lamp", null, draftLampMode, "manual".equals(draftLampMode) ? draftLampVal : null, null));
        lampCard.addView(applyLamp);
        tabContentContainer.addView(lampCard);

        // 2. Ventilasi Servo (0 - 90°)
        LinearLayout ventCard = makeCard();
        ventCard.addView(makeCardHeader("Servo Ventilasi Udara",
                "auto".equals(draftVentMode) ? "Mode Otomatis" : "Mode Manual", COL_GREEN));

        LinearLayout ventModeRow = makeModeSelectorRow(draftVentMode, mode -> {
            draftVentMode = mode;
            renderActiveTab();
        }, !stale);
        ventCard.addView(ventModeRow);

        if ("manual".equals(draftVentMode)) {
            TextView valLbl = makeBodyText("Sudut Bukaan: " + draftVentVal + "° (0° tutup – 90° buka penuh)");
            valLbl.setPadding(0, dp(10), 0, dp(4));
            ventCard.addView(valLbl);

            SeekBar sb = new SeekBar(this);
            sb.setMax(90);
            sb.setProgress(draftVentVal);
            sb.setEnabled(!stale);
            sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int p, boolean fromUser) {
                    draftVentVal = p;
                    valLbl.setText("Sudut Bukaan: " + p + "° (0° tutup – 90° buka penuh)");
                }
                @Override public void onStartTrackingTouch(SeekBar seekBar) {}
                @Override public void onStopTrackingTouch(SeekBar seekBar) {}
            });
            ventCard.addView(sb);
        } else {
            TextView note = makeBodyText("Bukaan servo dikendalikan otomatis sesuai suhu & kelembapan inkubator.");
            note.setPadding(0, dp(8), 0, dp(4));
            ventCard.addView(note);
        }

        Button applyVent = makePrimaryButton("Terapkan Ventilasi", COL_AMBER, Color.parseColor("#0F172A"));
        applyVent.setEnabled(!stale);
        applyVent.setAlpha(stale ? 0.45f : 1f);
        applyVent.setLayoutParams(lp1);
        applyVent.setOnClickListener(v -> sendDeviceCommand(
                "ventilation", null, draftVentMode, null, "manual".equals(draftVentMode) ? draftVentVal : null));
        ventCard.addView(applyVent);
        tabContentContainer.addView(ventCard);

        // 3. Pembalikan Telur Instan
        LinearLayout turnCard = makeCard();
        JSONObject inc = telemetry != null ? telemetry.optJSONObject("incubation") : null;
        JSONObject turning = telemetry != null ? telemetry.optJSONObject("turning") : null;
        JSONObject motor = telemetry != null ? telemetry.optJSONObject("motor") : null;

        String blockReason = "";
        if (stale) blockReason = "Perangkat tidak terhubung";
        else if (inc == null || !"running".equals(inc.optString("status"))) blockReason = "Inkubasi belum berjalan";
        else if ("lockdown".equals(inc.optString("stage")) || (turning != null && "lockdown".equals(turning.optString("status"))))
            blockReason = "Sedang fase Lockdown (Hari 19–21)";
        else if (turning != null && "fault".equals(turning.optString("status"))) blockReason = "Motor sedang gangguan";
        else if ((turning != null && "turning".equals(turning.optString("status"))) || (motor != null && motor.optBoolean("running")))
            blockReason = "Motor sedang berputar";

        turnCard.addView(makeCardHeader("Pembalik Telur (turn_now)",
                turning != null ? formatTurnLabel(turning.optString("status")) : "Idle", COL_TEXT));
        TextView tDesc = makeBodyText(blockReason.isEmpty()
                ? "Putar seluruh rak telur sekarang juga di luar jadwal rutin."
                : "Tidak tersedia: " + blockReason + ".");
        tDesc.setPadding(0, dp(8), 0, dp(10));
        turnCard.addView(tDesc);

        Button turnBtn = makePrimaryButton("Balik Telur Sekarang", COL_GREEN, Color.parseColor("#052E16"));
        boolean canTurn = blockReason.isEmpty();
        turnBtn.setEnabled(canTurn);
        turnBtn.setAlpha(canTurn ? 1f : 0.4f);
        turnBtn.setOnClickListener(v -> sendDeviceCommand("turning", "turn_now", null, null, null));
        turnCard.addView(turnBtn);
        tabContentContainer.addView(turnCard);
    }

    private void sendDeviceCommand(String target, String action, String mode, Integer intensity, Integer angle) {
        executor.execute(() -> {
            try {
                JSONObject body = new JSONObject();
                body.put("target", target);
                if (action != null) body.put("action", action);
                if (mode != null) body.put("mode", mode);
                if (intensity != null) body.put("intensity", intensity);
                if (angle != null) body.put("angle", angle);

                JSONObject res = api.postJson("/commands", body);
                long cmdId = res.optLong("id", 0);
                mainHandler.post(() -> {
                    Toast.makeText(this, "✓ Perintah #" + cmdId + " terkirim ke alat", Toast.LENGTH_SHORT).show();
                    fetchTelemetryNow();
                });
            } catch (Exception e) {
                mainHandler.post(() -> Toast.makeText(this, "⚠ Gagal: " + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        });
    }

    // =========================================================================
    // TAB 2: PERINGATAN (ALERTS)
    // =========================================================================
    private void renderAlertsTab() {
        LinearLayout filterRow = new LinearLayout(this);
        filterRow.setOrientation(LinearLayout.HORIZONTAL);
        filterRow.setPadding(0, 0, 0, dp(12));

        String[][] filters = {{"semua", "Semua"}, {"active", "Aktif"}, {"resolved", "Selesai"}};
        for (String[] f : filters) {
            final String key = f[0];
            boolean sel = key.equals(alertFilter);
            Button b = new Button(this);
            b.setText(f[1]);
            b.setAllCaps(false);
            b.setTextSize(12);
            b.setTextColor(sel ? Color.parseColor("#0F172A") : COL_TEXT);
            b.setBackground(roundedBox(sel ? COL_AMBER : COL_CARD, COL_LINE, 10));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(38), 1f);
            lp.rightMargin = dp(6);
            b.setLayoutParams(lp);
            b.setOnClickListener(v -> {
                alertFilter = key;
                renderActiveTab();
            });
            filterRow.addView(b);
        }
        tabContentContainer.addView(filterRow);

        int shown = 0;
        for (int i = 0; i < alerts.length(); i++) {
            JSONObject a = alerts.optJSONObject(i);
            if (a == null) continue;
            String st = a.optString("status", "resolved");
            if (!"semua".equals(alertFilter) && !alertFilter.equals(st)) continue;
            shown++;

            LinearLayout c = makeCard();
            String sev = a.optString("severity", "low");
            int col = "high".equals(sev) ? COL_RED : ("medium".equals(sev) ? COL_AMBER : COL_MUTED);
            c.addView(makeCardHeader(
                    a.optString("type", "alert").replace('_', ' ').toUpperCase(Locale.US),
                    "active".equals(st) ? "● AKTIF" : "✓ SELESAI",
                    "active".equals(st) ? col : COL_GREEN));

            TextView msg = makeBodyText(a.optString("message", "-"));
            msg.setTextColor(COL_TEXT);
            msg.setPadding(0, dp(6), 0, dp(4));
            c.addView(msg);

            TextView ts = makeBodyText("Waktu: " + a.optString("timestamp", "-"));
            ts.setTextSize(11);
            c.addView(ts);
            tabContentContainer.addView(c);
        }

        if (shown == 0) {
            tabContentContainer.addView(makeBodyText("Tidak ada peringatan untuk filter ini."));
        }
    }

    // =========================================================================
    // TAB 3: ATUR (PENGATURAN + TES NOTIFIKASI FIREBASE FCM)
    // =========================================================================
    private void renderSettingsTab() {
        boolean stale = isStaleOrOffline();

        // 1. Kartu Tes Push Notification Native & Firebase FCM (Selalu Aktif Walau Alat Offline!)
        LinearLayout notifCard = makeCard();
        notifCard.addView(makeCardHeader("Notifikasi Android & Firebase FCM", "AKTIF", COL_GREEN));
        TextView nDesc = makeBodyText("Aplikasi Native ini telah terhubung ke topik Firebase 'incubator_alerts'. Notifikasi tetap berbunyi walau aplikasi ditutup.");
        nDesc.setPadding(0, dp(6), 0, dp(10));
        notifCard.addView(nDesc);

        Button localTestBtn = makePrimaryButton("Uji Notifikasi Lokal HP", COL_CARD, COL_TEXT);
        localTestBtn.setOnClickListener(v -> HatcheryMessagingService.showPushNotification(
                this, "✓ Notifikasi Native Aktif", "Notifikasi lokal Smart Hatchery berfungsi dengan baik!", "uji-lokal"));
        notifCard.addView(localTestBtn);

        Button fcmTestBtn = makePrimaryButton("Kirim Uji Push dari Server VPS (Firebase FCM)", COL_GREEN, Color.parseColor("#052E16"));
        LinearLayout.LayoutParams fcmLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        fcmLp.topMargin = dp(8);
        fcmTestBtn.setLayoutParams(fcmLp);
        fcmTestBtn.setOnClickListener(v -> {
            fcmTestBtn.setEnabled(false);
            fcmTestBtn.setText("Mengirim lewat VPS -> Firebase…");
            executor.execute(() -> {
                try {
                    JSONObject r = api.postJson("/settings/notifications/test-fcm", new JSONObject());
                    mainHandler.post(() -> {
                        fcmTestBtn.setEnabled(true);
                        fcmTestBtn.setText("Kirim Uji Push dari Server VPS (Firebase FCM)");
                        Toast.makeText(this, "✓ " + r.optString("message", "Push terkirim!"), Toast.LENGTH_LONG).show();
                    });
                } catch (Exception e) {
                    mainHandler.post(() -> {
                        fcmTestBtn.setEnabled(true);
                        fcmTestBtn.setText("Kirim Uji Push dari Server VPS (Firebase FCM)");
                        Toast.makeText(this, "⚠ " + e.getMessage(), Toast.LENGTH_LONG).show();
                    });
                }
            });
        });
        notifCard.addView(fcmTestBtn);
        tabContentContainer.addView(notifCard);

        if (!settingsLoaded || deviceSettings == null) {
            tabContentContainer.addView(makeBodyText("Memuat pengaturan tersimpan dari database…"));
            return;
        }

        if (stale) {
            tabContentContainer.addView(makeBanner(
                    "⏱ Pengaturan perangkat dikunci sementara karena ESP32 sedang offline.", COL_AMBER));
        }

        // 2. Target Suhu (36.5 - 38.5 °C)
        LinearLayout tempCard = makeCard();
        tempCard.addView(makeCardHeader("Target Suhu Inkubasi", "36.5 – 38.5 °C", COL_AMBER));
        inputTargetTemp = makeEditText("37.5", InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
        inputTargetTemp.setText(String.valueOf(deviceSettings.optDouble("target_temperature", 37.5)));
        inputTargetTemp.setEnabled(!stale);
        LinearLayout.LayoutParams inLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        inLp.topMargin = dp(8);
        inLp.bottomMargin = dp(10);
        inputTargetTemp.setLayoutParams(inLp);
        tempCard.addView(inputTargetTemp);

        Button saveTempBtn = makePrimaryButton("Simpan Target Suhu", COL_AMBER, Color.parseColor("#0F172A"));
        saveTempBtn.setEnabled(!stale);
        saveTempBtn.setAlpha(stale ? 0.45f : 1f);
        saveTempBtn.setOnClickListener(v -> {
            try {
                double val = Double.parseDouble(inputTargetTemp.getText().toString().trim());
                JSONObject b = new JSONObject();
                b.put("value", val);
                putSetting("/settings/target-temperature", b, "Target suhu disimpan (" + val + "°C)");
            } catch (Exception ex) {
                Toast.makeText(this, "Masukkan angka suhu yang valid.", Toast.LENGTH_SHORT).show();
            }
        });
        tempCard.addView(saveTempBtn);
        tabContentContainer.addView(tempCard);

        // 3. Parameter Motor DC
        JSONObject mObj = deviceSettings.optJSONObject("motor");
        LinearLayout motorCard = makeCard();
        motorCard.addView(makeCardHeader("Parameter Motor DC", "Interval · Durasi · Speed", COL_TEXT));

        motorCard.addView(makeFieldLabel("Interval Pembalikan (30–480 menit)"));
        inputMotorInterval = makeEditText("120", InputType.TYPE_CLASS_NUMBER);
        inputMotorInterval.setText(String.valueOf(mObj != null ? mObj.optInt("interval_minutes", 120) : 120));
        inputMotorInterval.setEnabled(!stale);
        motorCard.addView(inputMotorInterval);

        motorCard.addView(makeFieldLabel("Durasi Putar (5–60 detik)"));
        inputMotorDuration = makeEditText("20", InputType.TYPE_CLASS_NUMBER);
        inputMotorDuration.setText(String.valueOf(mObj != null ? mObj.optInt("duration_seconds", 20) : 20));
        inputMotorDuration.setEnabled(!stale);
        motorCard.addView(inputMotorDuration);

        motorCard.addView(makeFieldLabel("Kecepatan Motor (10–70 %)"));
        inputMotorSpeed = makeEditText("50", InputType.TYPE_CLASS_NUMBER);
        inputMotorSpeed.setText(String.valueOf(mObj != null ? mObj.optInt("speed_percent", 50) : 50));
        inputMotorSpeed.setEnabled(!stale);
        motorCard.addView(inputMotorSpeed);

        Button saveMotorBtn = makePrimaryButton("Simpan Pengaturan Motor", COL_AMBER, Color.parseColor("#0F172A"));
        saveMotorBtn.setLayoutParams(fcmLp);
        saveMotorBtn.setEnabled(!stale);
        saveMotorBtn.setAlpha(stale ? 0.45f : 1f);
        saveMotorBtn.setOnClickListener(v -> {
            try {
                JSONObject b = new JSONObject();
                b.put("interval_minutes", Integer.parseInt(inputMotorInterval.getText().toString().trim()));
                b.put("duration_seconds", Integer.parseInt(inputMotorDuration.getText().toString().trim()));
                b.put("speed_percent", Integer.parseInt(inputMotorSpeed.getText().toString().trim()));
                putSetting("/settings/motor", b, "Pengaturan motor DC disimpan");
            } catch (Exception ex) {
                Toast.makeText(this, "Isi seluruh parameter motor dengan bilangan bulat.", Toast.LENGTH_SHORT).show();
            }
        });
        motorCard.addView(saveMotorBtn);
        tabContentContainer.addView(motorCard);

        // 4. Wi-Fi Perangkat (AES-256-GCM Write-Only)
        JSONObject wObj = deviceSettings.optJSONObject("wifi");
        LinearLayout wifiCard = makeCard();
        wifiCard.addView(makeCardHeader("Wi-Fi Perangkat ESP32", "Write-Only · Rollback 30s", COL_TEXT));
        wifiCard.addView(makeFieldLabel("Nama Wi-Fi (SSID 1–32 karakter)"));
        inputWifiSsid = makeEditText("SSID Wi-Fi", InputType.TYPE_CLASS_TEXT);
        inputWifiSsid.setText(wObj != null ? wObj.optString("ssid", "") : "");
        inputWifiSsid.setEnabled(!stale);
        wifiCard.addView(inputWifiSsid);

        wifiCard.addView(makeFieldLabel("Kata Sandi Baru (8–63 karakter)"));
        inputWifiPass = makeEditText("Isi untuk mengganti sandi Wi-Fi", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        inputWifiPass.setEnabled(!stale);
        wifiCard.addView(inputWifiPass);

        Button saveWifiBtn = makePrimaryButton("Kirim Wi-Fi ke ESP32", COL_RED, Color.WHITE);
        saveWifiBtn.setLayoutParams(fcmLp);
        saveWifiBtn.setEnabled(!stale);
        saveWifiBtn.setAlpha(stale ? 0.45f : 1f);
        saveWifiBtn.setOnClickListener(v -> {
            String ssid = inputWifiSsid.getText().toString().trim();
            String pass = inputWifiPass.getText().toString();
            if (ssid.isEmpty() || ssid.length() > 32 || pass.length() < 8 || pass.length() > 63) {
                Toast.makeText(this, "SSID wajib 1–32 karakter & kata sandi 8–63 karakter.", Toast.LENGTH_LONG).show();
                return;
            }
            new AlertDialog.Builder(this)
                    .setTitle("Kirim Wi-Fi Baru ke ESP32?")
                    .setMessage("ESP32 akan mencoba menyambung ke '" + ssid + "' maks. 30 detik (otomatis rollback ke Wi-Fi lama jika gagal).")
                    .setPositiveButton("Ya, Kirim", (d, w) -> {
                        try {
                            JSONObject b = new JSONObject();
                            b.put("ssid", ssid);
                            b.put("password", pass);
                            inputWifiPass.setText("");
                            putSetting("/settings/wifi", b, "Kredensial Wi-Fi terkirim (terenkripsi AES-256-GCM)");
                        } catch (Exception ignored) {}
                    })
                    .setNegativeButton("Batal", null)
                    .show();
        });
        wifiCard.addView(saveWifiBtn);
        tabContentContainer.addView(wifiCard);
    }

    private void putSetting(String path, JSONObject body, String okMsg) {
        executor.execute(() -> {
            try {
                api.putJson(path, body);
                mainHandler.post(() -> {
                    Toast.makeText(this, "✓ " + okMsg, Toast.LENGTH_LONG).show();
                    settingsLoaded = false;
                    fetchDeviceSettings();
                });
            } catch (Exception e) {
                mainHandler.post(() -> Toast.makeText(this, "⚠ " + e.getMessage(), Toast.LENGTH_LONG).show());
            }
        });
    }

    // =========================================================================
    // TAB 4: RIWAYAT AKTIVITAS (AUDIT LOGS)
    // =========================================================================
    private void renderAuditTab() {
        Button refreshBtn = makePrimaryButton("↻ Muat Ulang Riwayat Aktivitas", COL_CARD, COL_TEXT);
        LinearLayout.LayoutParams rLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rLp.bottomMargin = dp(12);
        refreshBtn.setLayoutParams(rLp);
        refreshBtn.setOnClickListener(v -> fetchAuditLogs());
        tabContentContainer.addView(refreshBtn);

        if (auditLogs.length() == 0) {
            tabContentContainer.addView(makeBodyText("Memuat atau belum ada catatan audit log…"));
            return;
        }

        for (int i = 0; i < auditLogs.length(); i++) {
            JSONObject l = auditLogs.optJSONObject(i);
            if (l == null) continue;
            LinearLayout c = makeCard();
            String ev = l.optString("event", "info");
            int col = "access_denied".equals(ev) ? COL_RED : ("command".equals(ev) ? COL_AMBER : COL_GREEN);
            c.addView(makeCardHeader(l.optString("user", "operator"), ev.toUpperCase(Locale.US), col));

            TextView det = makeBodyText(l.optString("detail", "-"));
            det.setTextColor(COL_TEXT);
            det.setPadding(0, dp(6), 0, dp(4));
            c.addView(det);

            TextView ts = makeBodyText(l.optString("timestamp", "-"));
            ts.setTextSize(11);
            c.addView(ts);
            tabContentContainer.addView(c);
        }
    }

    // =========================================================================
    // UI COMPONENT HELPERS
    // =========================================================================
    private interface ModeCallback {
        void onSelect(String mode);
    }

    private LinearLayout makeModeSelectorRow(String currentMode, ModeCallback cb, boolean enabled) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(10), 0, 0);

        String[][] opts = {{"auto", "Otomatis"}, {"manual", "Manual"}};
        for (String[] o : opts) {
            final String m = o[0];
            boolean sel = m.equals(currentMode);
            Button b = new Button(this);
            b.setText(o[1]);
            b.setAllCaps(false);
            b.setTextSize(13);
            b.setEnabled(enabled);
            b.setTextColor(sel ? Color.parseColor("#0F172A") : COL_TEXT);
            b.setBackground(roundedBox(sel ? COL_AMBER : COL_PANEL, COL_LINE, 10));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(40), 1f);
            if ("auto".equals(m)) lp.rightMargin = dp(8);
            b.setLayoutParams(lp);
            b.setOnClickListener(v -> cb.onSelect(m));
            row.addView(b);
        }
        return row;
    }

    private LinearLayout makeCard() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(roundedBox(COL_CARD, COL_LINE, 14));
        card.setPadding(dp(14), dp(14), dp(14), dp(14));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(12);
        card.setLayoutParams(lp);
        return card;
    }

    private LinearLayout makeCardHeader(String leftTitle, String badgeText, int badgeColor) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);

        TextView t = new TextView(this);
        t.setText(leftTitle);
        t.setTextColor(COL_TEXT);
        t.setTextSize(15);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(t);

        TextView b = new TextView(this);
        b.setText(badgeText);
        b.setTextColor(badgeColor);
        b.setTextSize(11);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        row.addView(b);
        return row;
    }

    private LinearLayout makeMetricRow(View leftTile, View rightTile) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams rowLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rowLp.bottomMargin = dp(10);
        row.setLayoutParams(rowLp);

        LinearLayout.LayoutParams lp1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f);
        lp1.rightMargin = dp(5);
        leftTile.setLayoutParams(lp1);

        LinearLayout.LayoutParams lp2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f);
        lp2.leftMargin = dp(5);
        rightTile.setLayoutParams(lp2);

        row.addView(leftTile);
        row.addView(rightTile);
        return row;
    }

    private LinearLayout makeMetricTile(String label, String bigValue, String subNote, int valueColor) {
        LinearLayout tile = new LinearLayout(this);
        tile.setOrientation(LinearLayout.VERTICAL);
        tile.setBackground(roundedBox(COL_CARD, COL_LINE, 14));
        tile.setPadding(dp(12), dp(12), dp(12), dp(12));

        TextView l = new TextView(this);
        l.setText(label);
        l.setTextColor(COL_MUTED);
        l.setTextSize(12);
        tile.addView(l);

        TextView v = new TextView(this);
        v.setText(bigValue);
        v.setTextColor(valueColor);
        v.setTextSize(20);
        v.setTypeface(Typeface.DEFAULT_BOLD);
        v.setPadding(0, dp(4), 0, dp(2));
        tile.addView(v);

        TextView s = new TextView(this);
        s.setText(subNote);
        s.setTextColor(COL_MUTED);
        s.setTextSize(11);
        tile.addView(s);
        return tile;
    }

    private TextView makeBanner(String msg, int borderColor) {
        TextView tv = new TextView(this);
        tv.setText(msg);
        tv.setTextColor(COL_TEXT);
        tv.setTextSize(12);
        tv.setBackground(roundedBox(COL_PANEL, borderColor, 12));
        tv.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(12);
        tv.setLayoutParams(lp);
        return tv;
    }

    private TextView makeSectionTitle(String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(COL_TEXT);
        tv.setTextSize(15);
        tv.setTypeface(Typeface.DEFAULT_BOLD);
        tv.setPadding(0, dp(4), 0, dp(8));
        return tv;
    }

    private TextView makeFieldLabel(String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(COL_MUTED);
        tv.setTextSize(12);
        tv.setPadding(0, dp(6), 0, dp(4));
        return tv;
    }

    private TextView makeBodyText(String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(COL_MUTED);
        tv.setTextSize(13);
        return tv;
    }

    private EditText makeEditText(String hint, int inputType) {
        EditText et = new EditText(this);
        et.setHint(hint);
        et.setHintTextColor(Color.parseColor("#64748B"));
        et.setTextColor(COL_TEXT);
        et.setTextSize(14);
        et.setInputType(inputType);
        et.setBackground(roundedBox(COL_BG, COL_LINE, 10));
        et.setPadding(dp(12), dp(10), dp(12), dp(10));
        return et;
    }

    private Button makePrimaryButton(String text, int bgColor, int textColor) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(14);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setTextColor(textColor);
        b.setBackground(roundedBox(bgColor, COL_LINE, 10));
        b.setPadding(dp(14), dp(10), dp(14), dp(10));
        return b;
    }

    private String formatTurnLabel(String st) {
        if ("turning".equals(st)) return "Memutar";
        if ("lockdown".equals(st)) return "Lockdown";
        if ("fault".equals(st)) return "Gangguan";
        return "Menunggu";
    }

    @Override
    protected void onDestroy() {
        stopPolling();
        executor.shutdownNow();
        super.onDestroy();
    }
}
