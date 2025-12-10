package com.example.webbrowser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.example.webbrowser.ui.TabAdapter;
import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class MainActivity extends AppCompatActivity implements TabAdapter.TabListener {
    private static final String PREFS_NAME = "mic_browser_prefs";
    private static final String KEY_BOOKMARKS = "bookmarks";
    private static final String KEY_HISTORY = "history";
    private static final int HISTORY_LIMIT = 100;

    private WebView webView;
    private EditText urlInput;
    private ProgressBar progressBar;
    private SwipeRefreshLayout refreshLayout;
    private RecyclerView tabList;
    private TabAdapter tabAdapter;
    private final List<Tab> tabs = new ArrayList<>();
    private final List<QuickLink> bookmarks = new ArrayList<>();
    private final List<QuickLink> history = new ArrayList<>();
    private Tab activeTab;
    private boolean desktopModeEnabled = false;
    private boolean incognitoModeEnabled = false;
    private boolean safeModeEnabled = true;
    private boolean trackingProtectionEnabled = true;
    private String defaultUserAgent;
    private SharedPreferences preferences;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        loadPersistedCollections();
        setupUi();
        setupWebView();
        createTab(getString(R.string.default_home));
        checkForUpdates(false);
    }

    private void setupUi() {
        MaterialToolbar toolbar = findViewById(R.id.topAppBar);
        toolbar.setSubtitle(getString(R.string.subtitle_complex));

        urlInput = findViewById(R.id.urlInput);
        progressBar = findViewById(R.id.progressBar);
        refreshLayout = findViewById(R.id.refreshLayout);
        tabList = findViewById(R.id.tabList);

        ImageButton backButton = findViewById(R.id.backButton);
        ImageButton forwardButton = findViewById(R.id.forwardButton);
        ImageButton refreshButton = findViewById(R.id.refreshButton);
        ImageButton newTabButton = findViewById(R.id.newTabButton);
        MaterialButton shareButton = findViewById(R.id.shareButton);
        MaterialButton copyButton = findViewById(R.id.copyButton);
        MaterialButton bookmarkButton = findViewById(R.id.bookmarkButton);
        MaterialButton historyButton = findViewById(R.id.historyButton);
        MaterialButton desktopButton = findViewById(R.id.desktopButton);
        MaterialButton incognitoButton = findViewById(R.id.incognitoButton);
        MaterialButton homeButton = findViewById(R.id.homeButton);
        MaterialButton safetyButton = findViewById(R.id.safetyButton);
        MaterialButton trackingButton = findViewById(R.id.trackingButton);
        MaterialButton purgeButton = findViewById(R.id.purgeButton);
        MaterialButton updateButton = findViewById(R.id.updateButton);

        tabAdapter = new TabAdapter(tabs, this);
        tabList.setLayoutManager(new LinearLayoutManager(this, RecyclerView.HORIZONTAL, false));
        tabList.setAdapter(tabAdapter);

        backButton.setOnClickListener(v -> {
            if (webView.canGoBack()) {
                webView.goBack();
            }
        });

        forwardButton.setOnClickListener(v -> {
            if (webView.canGoForward()) {
                webView.goForward();
            }
        });

        refreshButton.setOnClickListener(v -> webView.reload());
        newTabButton.setOnClickListener(v -> createTab(null));
        shareButton.setOnClickListener(v -> shareCurrentPage());
        copyButton.setOnClickListener(v -> copyCurrentUrl());
        bookmarkButton.setOnClickListener(v -> addBookmarkFromCurrent());
        bookmarkButton.setOnLongClickListener(v -> {
            showCollectionDialog(getString(R.string.label_bookmarks), bookmarks);
            return true;
        });
        historyButton.setOnClickListener(v -> showCollectionDialog(getString(R.string.label_history), history));
        desktopButton.setOnClickListener(v -> toggleDesktopMode((MaterialButton) v));
        incognitoButton.setOnClickListener(v -> toggleIncognitoMode((MaterialButton) v));
        homeButton.setOnClickListener(v -> loadUrl(getString(R.string.default_home)));
        safetyButton.setOnClickListener(v -> toggleSafeMode((MaterialButton) v));
        trackingButton.setOnClickListener(v -> toggleTrackingProtection((MaterialButton) v));
        purgeButton.setOnClickListener(v -> purgeSession());
        updateButton.setOnClickListener(v -> checkForUpdates(true));

        refreshLayout.setOnRefreshListener(() -> {
            webView.reload();
            refreshLayout.setRefreshing(false);
        });

        urlInput.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO || actionId == EditorInfo.IME_ACTION_DONE ||
                    (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                loadFromInput(urlInput.getText().toString());
                return true;
            }
            return false;
        });
    }

    private void setupWebView() {
        webView = findViewById(R.id.webView);
        WebSettings settings = webView.getSettings();
        defaultUserAgent = settings.getUserAgentString();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setBuiltInZoomControls(true);
        settings.setDisplayZoomControls(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        applyTrackingProtection();

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                progressBar.setProgress(newProgress);
                progressBar.setVisibility(newProgress == 100 ? View.GONE : View.VISIBLE);
            }

            @Override
            public void onReceivedTitle(WebView view, String title) {
                updateActiveTabTitle(title);
            }
        });

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                handleNavigationRequest(request.getUrl().toString());
                return true;
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                urlInput.setText(url);
                updateActiveTabUrl(url);
                logHistory(view.getTitle(), url);
            }
        });
    }

    private void loadFromInput(String rawInput) {
        if (TextUtils.isEmpty(rawInput)) {
            return;
        }
        String url = rawInput.trim();
        if (!url.startsWith("http")) {
            url = "https://duckduckgo.com/?q=" + Uri.encode(url);
        }
        loadUrl(url);
    }

    private void loadUrl(String url) {
        handleNavigationRequest(url);
    }

    private void createTab(@Nullable String initialUrl) {
        String home = getString(R.string.default_home);
        String url = TextUtils.isEmpty(initialUrl) ? home : initialUrl;
        Tab tab = new Tab(UUID.randomUUID().toString(), getString(R.string.label_new_tab), url);
        tabs.add(tab);
        activeTab = tab;
        tabAdapter.setSelectedTabId(tab.getId());
        tabAdapter.notifyItemInserted(tabs.size() - 1);
        handleNavigationRequest(url);
        scrollTabsToEnd();
    }

    private void closeTab(Tab tab) {
        int index = tabs.indexOf(tab);
        if (index >= 0) {
            tabs.remove(index);
            tabAdapter.notifyItemRemoved(index);
        }
        if (tabs.isEmpty()) {
            createTab(getString(R.string.default_home));
        } else {
            activeTab = tabs.get(Math.max(0, index - 1));
            tabAdapter.setSelectedTabId(activeTab.getId());
            loadUrl(activeTab.getUrl());
        }
    }

    private void updateActiveTabTitle(String title) {
        if (activeTab != null) {
            activeTab.setTitle(title);
            tabAdapter.notifyDataSetChanged();
        }
    }

    private void updateActiveTabUrl(String url) {
        if (activeTab != null) {
            activeTab.setUrl(url);
            tabAdapter.notifyDataSetChanged();
        }
    }

    private void shareCurrentPage() {
        if (activeTab == null || TextUtils.isEmpty(activeTab.getUrl())) {
            return;
        }
        Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.setType("text/plain");
        shareIntent.putExtra(Intent.EXTRA_TEXT, activeTab.getUrl());
        startActivity(Intent.createChooser(shareIntent, getString(R.string.action_share)));
    }

    private void copyCurrentUrl() {
        if (activeTab == null || TextUtils.isEmpty(activeTab.getUrl())) {
            return;
        }
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("URL", activeTab.getUrl());
        clipboard.setPrimaryClip(clip);
        Toast.makeText(this, "Link copied", Toast.LENGTH_SHORT).show();
    }

    private void scrollTabsToEnd() {
        tabList.post(() -> tabList.smoothScrollToPosition(tabAdapter.getItemCount() - 1));
    }

    private void addBookmarkFromCurrent() {
        if (activeTab == null || TextUtils.isEmpty(activeTab.getUrl())) {
            return;
        }
        QuickLink link = new QuickLink(activeTab.getTitle(), activeTab.getUrl());
        if (!bookmarks.contains(link)) {
            bookmarks.add(link);
            saveCollection(KEY_BOOKMARKS, bookmarks);
            Toast.makeText(this, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(this, R.string.bookmark_exists, Toast.LENGTH_SHORT).show();
        }
    }

    private void showCollectionDialog(String title, List<QuickLink> items) {
        if (items.isEmpty()) {
            Toast.makeText(this, R.string.empty_collection, Toast.LENGTH_SHORT).show();
            return;
        }
        CharSequence[] entries = new CharSequence[items.size()];
        for (int i = 0; i < items.size(); i++) {
            QuickLink link = items.get(i);
            entries[i] = link.getTitle() + "\n" + link.getUrl();
        }
        new AlertDialog.Builder(this)
                .setTitle(title)
                .setItems(entries, (dialog, which) -> loadUrl(items.get(which).getUrl()))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void logHistory(String pageTitle, String url) {
        if (incognitoModeEnabled || TextUtils.isEmpty(url)) {
            return;
        }
        String title = TextUtils.isEmpty(pageTitle) ? url : pageTitle;
        QuickLink link = new QuickLink(title, url);
        history.add(0, link);
        if (history.size() > HISTORY_LIMIT) {
            history.subList(HISTORY_LIMIT, history.size()).clear();
        }
        saveCollection(KEY_HISTORY, history);
    }

    private void toggleDesktopMode(MaterialButton button) {
        desktopModeEnabled = !desktopModeEnabled;
        WebSettings settings = webView.getSettings();
        if (desktopModeEnabled) {
            settings.setUserAgentString("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Mobile Safari/537.36");
            settings.setUseWideViewPort(true);
            settings.setLoadWithOverviewMode(true);
            button.setText(R.string.action_desktop_on);
        } else {
            settings.setUserAgentString(defaultUserAgent);
            button.setText(R.string.action_desktop_off);
        }
        webView.reload();
    }

    private void toggleIncognitoMode(MaterialButton button) {
        incognitoModeEnabled = !incognitoModeEnabled;
        WebSettings settings = webView.getSettings();
        CookieManager cookieManager = CookieManager.getInstance();
        if (incognitoModeEnabled) {
            settings.setCacheMode(WebSettings.LOAD_NO_CACHE);
            settings.setSavePassword(false);
            settings.setSaveFormData(false);
            webView.clearHistory();
            webView.clearCache(true);
            cookieManager.removeAllCookies(null);
            cookieManager.flush();
            button.setText(R.string.action_incognito_on);
            Toast.makeText(this, R.string.incognito_on, Toast.LENGTH_SHORT).show();
        } else {
            settings.setCacheMode(WebSettings.LOAD_DEFAULT);
            cookieManager.flush();
            button.setText(R.string.action_incognito_off);
            Toast.makeText(this, R.string.incognito_off, Toast.LENGTH_SHORT).show();
        }
    }

    private void toggleSafeMode(MaterialButton button) {
        safeModeEnabled = !safeModeEnabled;
        button.setText(safeModeEnabled ? R.string.action_safe_on : R.string.action_safe_off);
        Toast.makeText(this, safeModeEnabled ? R.string.toast_safe_on : R.string.toast_safe_off, Toast.LENGTH_SHORT).show();
    }

    private void toggleTrackingProtection(MaterialButton button) {
        trackingProtectionEnabled = !trackingProtectionEnabled;
        applyTrackingProtection();
        button.setText(trackingProtectionEnabled ? R.string.action_tracking_on : R.string.action_tracking_off);
        Toast.makeText(this, trackingProtectionEnabled ? R.string.toast_tracking_on : R.string.toast_tracking_off, Toast.LENGTH_SHORT).show();
    }

    private void purgeSession() {
        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.removeAllCookies(null);
        cookieManager.flush();
        webView.clearHistory();
        webView.clearCache(true);
        history.clear();
        bookmarks.clear();
        saveCollection(KEY_HISTORY, history);
        saveCollection(KEY_BOOKMARKS, bookmarks);
        Toast.makeText(this, R.string.toast_purged, Toast.LENGTH_SHORT).show();
    }

    private void applyTrackingProtection() {
        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.setAcceptThirdPartyCookies(webView, !trackingProtectionEnabled);
        cookieManager.setAcceptCookie(true);
        webView.getSettings().setBlockNetworkLoads(false);
    }

    private void loadPersistedCollections() {
        bookmarks.clear();
        history.clear();
        bookmarks.addAll(readCollection(KEY_BOOKMARKS));
        history.addAll(readCollection(KEY_HISTORY));
    }

    private List<QuickLink> readCollection(String key) {
        List<QuickLink> list = new ArrayList<>();
        String raw = preferences.getString(key, "");
        if (TextUtils.isEmpty(raw)) {
            return list;
        }
        try {
            JSONArray array = new JSONArray(raw);
            for (int i = 0; i < array.length(); i++) {
                JSONObject obj = array.getJSONObject(i);
                list.add(QuickLink.fromJson(obj));
            }
        } catch (JSONException e) {
            // Ignore malformed cache; start fresh.
        }
        return list;
    }

    private void saveCollection(String key, List<QuickLink> items) {
        JSONArray array = new JSONArray();
        for (QuickLink link : items) {
            array.put(link.toJson());
        }
        preferences.edit().putString(key, array.toString()).apply();
    }

    @Override
    public void onTabSelected(Tab tab) {
        activeTab = tab;
        tabAdapter.setSelectedTabId(tab.getId());
        handleNavigationRequest(tab.getUrl());
    }

    @Override
    public void onTabClosed(Tab tab) {
        closeTab(tab);
    }

    @Override
    public void onBackPressed() {
        if (webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    private void handleNavigationRequest(String rawInput) {
        if (TextUtils.isEmpty(rawInput)) {
            return;
        }
        String normalized = normalizeUrl(rawInput.trim());
        if (safeModeEnabled) {
            String warning = getSafetyWarning(normalized);
            if (!TextUtils.isEmpty(warning)) {
                showUnsafeDialog(normalized, warning);
                return;
            }
        }
        performNavigation(normalized);
    }

    private String normalizeUrl(String input) {
        String url = input;
        if (!url.startsWith("http")) {
            url = "https://" + url;
        }
        if (safeModeEnabled && url.startsWith("http://")) {
            url = url.replaceFirst("http://", "https://");
        }
        return url;
    }

    private void performNavigation(String url) {
        webView.loadUrl(url);
        urlInput.setText(url);
        updateActiveTabUrl(url);
    }

    private void showUnsafeDialog(String url, String reason) {
        new AlertDialog.Builder(this)
                .setTitle(R.string.title_security_gate)
                .setMessage(getString(R.string.message_security_gate, reason))
                .setPositiveButton(R.string.action_proceed_anyway, (dialog, which) -> performNavigation(url))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private String getSafetyWarning(String url) {
        Uri uri = Uri.parse(url);
        String host = uri.getHost();
        if (TextUtils.isEmpty(host)) {
            return getString(R.string.reason_unknown_host);
        }
        if (!url.startsWith("https")) {
            return getString(R.string.reason_unencrypted);
        }
        String lowerHost = host.toLowerCase();
        String[] suspectKeywords = {"phish", "malware", "tracking", "ads"};
        for (String keyword : suspectKeywords) {
            if (lowerHost.contains(keyword)) {
                return getString(R.string.reason_flagged_host, keyword);
            }
        }
        return null;
    }

    private void checkForUpdates(boolean userInitiated) {
        new Thread(() -> {
            UpdateInfo info = fetchUpdateInfo();
            runOnUiThread(() -> {
                if (!info.success) {
                    if (userInitiated) {
                        Toast.makeText(this, info.message, Toast.LENGTH_SHORT).show();
                    }
                    return;
                }
                if (info.versionCode > BuildConfig.VERSION_CODE) {
                    showUpdateDialog(info);
                } else if (userInitiated) {
                    Toast.makeText(this, R.string.toast_update_current, Toast.LENGTH_SHORT).show();
                }
            });
        }).start();
    }

    private UpdateInfo fetchUpdateInfo() {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(getString(R.string.update_manifest_url));
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(5000);
            connection.setReadTimeout(5000);
            connection.setRequestProperty("Accept", "application/json");
            int code = connection.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) {
                return UpdateInfo.error(getString(R.string.toast_update_error, code));
            }
            String body = slurp(connection.getInputStream());
            JSONObject json = new JSONObject(body);
            int versionCode = json.optInt("versionCode", 0);
            String versionName = json.optString("versionName", "");
            String downloadUrl = json.optString("downloadUrl", "");
            String notes = json.optString("changelog", getString(R.string.label_no_changelog));
            return UpdateInfo.success(versionCode, versionName, downloadUrl, notes);
        } catch (IOException | JSONException e) {
            return UpdateInfo.error(getString(R.string.toast_update_failed));
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private void showUpdateDialog(UpdateInfo info) {
        String versionLabel = TextUtils.isEmpty(info.versionName) ? String.valueOf(info.versionCode) : info.versionName;
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.title_update_available, versionLabel))
                .setMessage(getString(R.string.message_update_available, versionLabel, info.notes))
                .setPositiveButton(R.string.action_update_now, (dialog, which) -> {
                    if (!TextUtils.isEmpty(info.downloadUrl)) {
                        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(info.downloadUrl));
                        startActivity(intent);
                    }
                })
                .setNegativeButton(R.string.action_later, null)
                .show();
    }

    private String slurp(InputStream inputStream) throws IOException {
        StringBuilder builder = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                builder.append(line);
            }
        }
        return builder.toString();
    }

    private static class UpdateInfo {
        final boolean success;
        final int versionCode;
        final String versionName;
        final String downloadUrl;
        final String notes;
        final String message;

        private UpdateInfo(boolean success, int versionCode, String versionName, String downloadUrl, String notes, String message) {
            this.success = success;
            this.versionCode = versionCode;
            this.versionName = versionName;
            this.downloadUrl = downloadUrl;
            this.notes = notes;
            this.message = message;
        }

        static UpdateInfo success(int versionCode, String versionName, String downloadUrl, String notes) {
            return new UpdateInfo(true, versionCode, versionName, downloadUrl, notes, "");
        }

        static UpdateInfo error(String message) {
            return new UpdateInfo(false, 0, "", "", "", message);
        }
    }

    private static class QuickLink {
        private final String title;
        private final String url;

        QuickLink(String title, String url) {
            this.title = TextUtils.isEmpty(title) ? url : title;
            this.url = url;
        }

        String getTitle() {
            return title;
        }

        String getUrl() {
            return url;
        }

        JSONObject toJson() {
            JSONObject object = new JSONObject();
            try {
                object.put("title", title);
                object.put("url", url);
            } catch (JSONException ignored) {
            }
            return object;
        }

        static QuickLink fromJson(JSONObject object) throws JSONException {
            return new QuickLink(object.getString("title"), object.getString("url"));
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof QuickLink)) {
                return false;
            }
            QuickLink other = (QuickLink) obj;
            return TextUtils.equals(title, other.title) && TextUtils.equals(url, other.url);
        }

        @Override
        public int hashCode() {
            return (title + url).hashCode();
        }
    }
}
