package id.iteba.smarthatchery;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Map;

public class ApiClient {

    public static final String DEFAULT_BASE_URL = "https://app.itebaincubatortelur.cc.cd";
    private static final String PREFS = "sh_native_prefs";
    private static final String KEY_COOKIE = "session_cookie";
    private static final String KEY_BASE_URL = "base_url";

    private final SharedPreferences prefs;

    public static class ApiException extends Exception {
        public final int statusCode;
        public ApiException(int statusCode, String message) {
            super(message);
            this.statusCode = statusCode;
        }
    }

    public ApiClient(Context ctx) {
        this.prefs = ctx.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    public String getBaseUrl() {
        return prefs.getString(KEY_BASE_URL, DEFAULT_BASE_URL);
    }

    public void clearSession() {
        prefs.edit().remove(KEY_COOKIE).apply();
    }

    public String rawRequest(String method, String path, String jsonBody) throws Exception {
        String base = getBaseUrl();
        URL url = new URL(base + "/api" + path);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod(method);
        conn.setConnectTimeout(8000);
        conn.setReadTimeout(10000);
        conn.setRequestProperty("Accept", "application/json");
        conn.setRequestProperty("Origin", base);

        String savedCookie = prefs.getString(KEY_COOKIE, "");
        if (!savedCookie.isEmpty()) {
            conn.setRequestProperty("Cookie", savedCookie);
        }

        if (jsonBody != null) {
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] bytes = jsonBody.getBytes(StandardCharsets.UTF_8);
            try (OutputStream os = conn.getOutputStream()) {
                os.write(bytes);
            }
        }

        int status = conn.getResponseCode();

        // Persist Set-Cookie header (session_id=...)
        Map<String, List<String>> headers = conn.getHeaderFields();
        if (headers != null) {
            for (Map.Entry<String, List<String>> entry : headers.entrySet()) {
                if (entry.getKey() != null && entry.getKey().equalsIgnoreCase("Set-Cookie")) {
                    for (String c : entry.getValue()) {
                        String pair = c.split(";")[0].trim();
                        if (pair.startsWith("session_id=")) {
                            if (pair.equals("session_id=") || c.contains("Max-Age=0")) {
                                clearSession();
                            } else {
                                prefs.edit().putString(KEY_COOKIE, pair).apply();
                            }
                        }
                    }
                }
            }
        }

        InputStream is = (status >= 200 && status < 300) ? conn.getInputStream() : conn.getErrorStream();
        String respText = "";
        if (is != null) {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = is.read(buf)) != -1) {
                baos.write(buf, 0, n);
            }
            is.close();
            respText = baos.toString("UTF-8");
        }
        conn.disconnect();

        if (status < 200 || status >= 300) {
            if (status == 401 && !path.startsWith("/auth/login")) {
                clearSession();
            }
            String errMsg = "Permintaan gagal (HTTP " + status + ")";
            try {
                JSONObject errObj = new JSONObject(respText);
                if (errObj.has("message")) {
                    errMsg = errObj.getString("message");
                }
            } catch (Exception ignored) {}
            throw new ApiException(status, errMsg);
        }
        return respText;
    }

    public JSONObject getJson(String path) throws Exception {
        String raw = rawRequest("GET", path, null);
        return raw.isEmpty() ? new JSONObject() : new JSONObject(raw);
    }

    public JSONArray getJsonArray(String path) throws Exception {
        String raw = rawRequest("GET", path, null);
        return raw.isEmpty() ? new JSONArray() : new JSONArray(raw);
    }

    public JSONObject postJson(String path, JSONObject body) throws Exception {
        String raw = rawRequest("POST", path, body != null ? body.toString() : "{}");
        return raw.isEmpty() ? new JSONObject() : new JSONObject(raw);
    }

    public JSONObject putJson(String path, JSONObject body) throws Exception {
        String raw = rawRequest("PUT", path, body != null ? body.toString() : "{}");
        return raw.isEmpty() ? new JSONObject() : new JSONObject(raw);
    }
}
