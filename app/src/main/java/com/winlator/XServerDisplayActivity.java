package com.winlator;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.Spinner;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.preference.PreferenceManager;

import com.google.android.material.navigation.NavigationView;
import com.winlator.container.Container;
import com.winlator.container.ContainerManager;
import com.winlator.container.Shortcut;
import com.winlator.contentdialog.ContentDialog;
import com.winlator.contentdialog.DXVKConfigDialog;
import com.winlator.contentdialog.DebugDialog;
import com.winlator.core.AppUtils;
import com.winlator.core.DefaultVersion;
import com.winlator.core.EnvVars;
import com.winlator.core.FileUtils;
import com.winlator.core.GPUInformation;
import com.winlator.core.KeyValueSet;
import com.winlator.core.OnExtractFileListener;
import com.winlator.core.PreloaderDialog;
import com.winlator.core.ProcessHelper;
import com.winlator.core.TarCompressorUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineRegistryEditor;
import com.winlator.core.WineStartMenuCreator;
import com.winlator.core.WineThemeManager;
import com.winlator.core.WineUtils;
import com.winlator.inputcontrols.ControlsProfile;
import com.winlator.inputcontrols.ExternalController;
import com.winlator.inputcontrols.InputControlsManager;
import com.winlator.math.Mathf;
import com.winlator.renderer.GLRenderer;
import com.winlator.widget.FrameRating;
import com.winlator.widget.InputControlsView;
import com.winlator.widget.MagnifierView;
import com.winlator.widget.TouchpadView;
import com.winlator.widget.XServerView;
import com.winlator.winhandler.TaskManagerDialog;
import com.winlator.winhandler.WinHandler;
import com.winlator.xconnector.UnixSocketConfig;
import com.winlator.xenvironment.ImageFs;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xenvironment.components.ALSAServerComponent;
import com.winlator.xenvironment.components.GuestProgramLauncherComponent;
import com.winlator.xenvironment.components.NetworkInfoUpdateComponent;
import com.winlator.xenvironment.components.PulseAudioComponent;
import com.winlator.xenvironment.components.SysVSharedMemoryComponent;
import com.winlator.xenvironment.components.VirGLRendererComponent;
import com.winlator.xenvironment.components.XServerComponent;
import com.winlator.xserver.Property;
import com.winlator.xserver.ScreenInfo;
import com.winlator.xserver.Window;
import com.winlator.xserver.WindowManager;
import com.winlator.xserver.XServer;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Executors;

public class XServerDisplayActivity extends AppCompatActivity implements NavigationView.OnNavigationItemSelectedListener {
    private XServerView xServerView;
    private InputControlsView inputControlsView;
    private TouchpadView touchpadView;
    private XEnvironment environment;
    private DrawerLayout drawerLayout;
    private ContainerManager containerManager;
    private Container container;
    private XServer xServer;
    private InputControlsManager inputControlsManager;
    private ImageFs imageFs;
    private FrameRating frameRating;
    private Runnable editInputControlsCallback;
    private Shortcut shortcut;
    private String graphicsDriver = Container.DEFAULT_GRAPHICS_DRIVER;
    private String audioDriver = Container.DEFAULT_AUDIO_DRIVER;
    private String dxwrapper = Container.DEFAULT_DXWRAPPER;
    private KeyValueSet dxwrapperConfig;
    private WineInfo wineInfo;
    private final EnvVars envVars = new EnvVars();
    private boolean firstTimeBoot = false;
    private SharedPreferences preferences;
    private OnExtractFileListener onExtractFileListener;
    private final WinHandler winHandler = new WinHandler(this);
    private float globalCursorSpeed = 1.0f;
    private boolean globalCursorDirect = false;
    private MagnifierView magnifierView;
    private DebugDialog debugDialog;
    private short taskAffinityMask = 0;
    private short taskAffinityMaskWoW64 = 0;
    private int frameRatingWindowId = -1;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        AppUtils.hideSystemUI(this);
        AppUtils.keepScreenOn(this);
        setContentView(R.layout.xserver_display_activity);

        final PreloaderDialog preloaderDialog = new PreloaderDialog(this);
        preferences = PreferenceManager.getDefaultSharedPreferences(this);

        drawerLayout = findViewById(R.id.DrawerLayout);
        drawerLayout.setOnApplyWindowInsetsListener((view, windowInsets) -> windowInsets.replaceSystemWindowInsets(0, 0, 0, 0));
        drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);

        NavigationView navigationView = findViewById(R.id.NavigationView);
        ProcessHelper.removeAllDebugCallbacks();
        boolean enableLogs = preferences.getBoolean("enable_wine_debug", false) || preferences.getBoolean("enable_box86_64_logs", false);
        if (enableLogs) ProcessHelper.addDebugCallback(debugDialog = new DebugDialog(this));
        Menu menu = navigationView.getMenu();
        menu.findItem(R.id.main_menu_logs).setVisible(enableLogs);
        if (XrActivity.isSupported()) menu.findItem(R.id.main_menu_magnifier).setVisible(false);
        navigationView.setNavigationItemSelectedListener(this);

        imageFs = ImageFs.find(this);

        String screenSize = Container.DEFAULT_SCREEN_SIZE;
        if (!isGenerateWineprefix()) {
            containerManager = new ContainerManager(this);
            container = containerManager.getContainerById(getIntent().getIntExtra("container_id", 0));
            containerManager.activateContainer(container);

            boolean wineprefixNeedsUpdate = container.getExtra("wineprefixNeedsUpdate").equals("t");
            if (wineprefixNeedsUpdate) {
                preloaderDialog.show(R.string.updating_system_files);
                WineUtils.updateWineprefix(this, (status) -> {
                    if (status == 0) {
                        container.putExtra("wineprefixNeedsUpdate", null);
                        container.putExtra("wincomponents", null);
                        container.saveData();
                        AppUtils.restartActivity(this);
                    }
                    else finish();
                });
                return;
            }

            taskAffinityMask = (short)ProcessHelper.getAffinityMask(container.getCPUList(true));
            taskAffinityMaskWoW64 = (short)ProcessHelper.getAffinityMask(container.getCPUListWoW64(true));
            firstTimeBoot = container.getExtra("appVersion").isEmpty();

            String wineVersion = container.getWineVersion();
            wineInfo = WineInfo.fromIdentifier(this, wineVersion);

            if (wineInfo != WineInfo.MAIN_WINE_VERSION) imageFs.setWinePath(wineInfo.path);

            String shortcutPath = getIntent().getStringExtra("shortcut_path");
            if (shortcutPath != null && !shortcutPath.isEmpty()) shortcut = new Shortcut(container, new File(shortcutPath));

            graphicsDriver = container.getGraphicsDriver();
            audioDriver = container.getAudioDriver();
            dxwrapper = container.getDXWrapper();
            String dxwrapperConfig = container.getDXWrapperConfig();
            screenSize = container.getScreenSize();

            if (shortcut != null) {
                graphicsDriver = shortcut.getExtra("graphicsDriver", container.getGraphicsDriver());
                audioDriver = shortcut.getExtra("audioDriver", container.getAudioDriver());
                dxwrapper = shortcut.getExtra("dxwrapper", container.getDXWrapper());
                dxwrapperConfig = shortcut.getExtra("dxwrapperConfig", container.getDXWrapperConfig());
                screenSize = shortcut.getExtra("screenSize", container.getScreenSize());

                String dinputMapperType = shortcut.getExtra("dinputMapperType");
                if (!dinputMapperType.isEmpty()) winHandler.setDInputMapperType(Byte.parseByte(dinputMapperType));
            }

            if (dxwrapper.equals("dxvk")) this.dxwrapperConfig = DXVKConfigDialog.parseConfig(dxwrapperConfig);

            if (!wineInfo.isWin64()) {
                onExtractFileListener = (file, size) -> {
                    String path = file.getPath();
                    if (path.contains("system32/")) return null;
                    return new File(path.replace("syswow64/", "system32/"));
                };
            }
        }

        preloaderDialog.show(R.string.starting_up);

        inputControlsManager = new InputControlsManager(this);
        xServer = new XServer(new ScreenInfo(screenSize));
        xServer.setWinHandler(winHandler);
        boolean[] winStarted = {false};
        xServer.windowManager.addOnWindowModificationListener(new WindowManager.OnWindowModificationListener() {
            @Override
            public void onUpdateWindowContent(Window window) {
                if (!winStarted[0] && window.isApplicationWindow()) {
                    xServerView.getRenderer().setCursorVisible(true);
                    preloaderDialog.closeOnUiThread();
                    winStarted[0] = true;
                }

                if (window.id == frameRatingWindowId) frameRating.update();
            }

            @Override
            public void onModifyWindowProperty(Window window, Property property) {
                changeFrameRatingVisibility(window, property);
            }

            @Override
            public void onMapWindow(Window window) {
                assignTaskAffinity(window);
            }

            @Override
            public void onUnmapWindow(Window window) {
                changeFrameRatingVisibility(window, null);
            }
        });

        setupUI();

        Executors.newSingleThreadExecutor().execute(() -> {
            if (!isGenerateWineprefix()) {
                setupWineSystemFiles();
                extractGraphicsDriverFiles();
                changeWineAudioDriver();
            }
            setupXEnvironment();
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == MainActivity.EDIT_INPUT_CONTROLS_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            if (editInputControlsCallback != null) {
                editInputControlsCallback.run();
                editInputControlsCallback = null;
            }
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (environment != null) {
            xServerView.onResume();
            environment.onResume();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (environment != null) {
            environment.onPause();
            xServerView.onPause();
        }
    }

    @Override
    protected void onDestroy() {
        winHandler.stop();
        if (environment != null) environment.stopEnvironmentComponents();
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if (environment != null) {
            if (!drawerLayout.isDrawerOpen(GravityCompat.START)) {
                drawerLayout.openDrawer(GravityCompat.START);
            }
            else drawerLayout.closeDrawers();
        }
    }

    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        final GLRenderer renderer = xServerView.getRenderer();
        switch (item.getItemId()) {
            case R.id.main_menu_keyboard:
                AppUtils.showKeyboard(this);
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_input_controls:
                showInputControlsDialog();
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_toggle_fullscreen:
                renderer.toggleFullscreen();
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_task_manager:
                (new TaskManagerDialog(this)).show();
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_magnifier:
                if (magnifierView == null) {
                    final FrameLayout container = findViewById(R.id.FLXServerDisplay);
                    magnifierView = new MagnifierView(this);
                    magnifierView.setZoomButtonCallback((value) -> {
                        renderer.setMagnifierZoom(Mathf.clamp(renderer.getMagnifierZoom() + value, 1.0f, 3.0f));
                        magnifierView.setZoomValue(renderer.getMagnifierZoom());
                    });
                    magnifierView.setZoomValue(renderer.getMagnifierZoom());
                    magnifierView.setHideButtonCallback(() -> {
                        container.removeView(magnifierView);
                        magnifierView = null;
                    });
                    container.addView(magnifierView);
                }
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_logs:
                debugDialog.show();
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_touchpad_help:
                showTouchpadHelpDialog();
                break;
            case R.id.main_menu_exit:
                exit();
                break;
        }
        return true;
    }

    private void exit() {
        winHandler.stop();
        if (environment != null) environment.stopEnvironmentComponents();
        AppUtils.restartApplication(this);
    }

    private void setupWineSystemFiles() {
        String appVersion = String.valueOf(AppUtils.getVersionCode(this));
        String imgVersion = String.valueOf(imageFs.getVersion());
        boolean containerDataChanged = false;

        if (!container.getExtra("appVersion").equals(appVersion) || !container.getExtra("imgVersion").equals(imgVersion)) {
            applyGeneralPatches(container);
            container.putExtra("appVersion", appVersion);
            container.putExtra("imgVersion", imgVersion);
            containerDataChanged = true;
        }

        String dxwrapper = this.dxwrapper;
        if (dxwrapper.equals("dxvk")) dxwrapper = "dxvk-"+dxwrapperConfig.get("version");

        if (!dxwrapper.equals(container.getExtra("dxwrapper"))) {
            extractDXWrapperFiles(dxwrapper);
            container.putExtra("dxwrapper", dxwrapper);
            containerDataChanged = true;
        }

        if (dxwrapper.equals("cnc-ddraw")) envVars.put("CNC_DDRAW_CONFIG_FILE", "C:\\ProgramData\\cnc-ddraw\\ddraw.ini");

        String wincomponents = shortcut != null ? shortcut.getExtra("wincomponents", container.getWinComponents()) : container.getWinComponents();
        if (!wincomponents.equals(container.getExtra("wincomponents"))) {
            extractWinComponentFiles();
            container.putExtra("wincomponents", wincomponents);
            containerDataChanged = true;
        }

        String desktopTheme = container.getDesktopTheme();
        if (!(desktopTheme+","+xServer.screenInfo).equals(container.getExtra("desktopTheme"))) {
            WineThemeManager.apply(this, new WineThemeManager.ThemeInfo(desktopTheme), xServer.screenInfo);
            container.putExtra("desktopTheme", desktopTheme+","+xServer.screenInfo);
            containerDataChanged = true;
        }

        WineStartMenuCreator.create(this, container);
        WineUtils.createDosdevicesSymlinks(container);

        String startupSelection = String.valueOf(container.getStartupSelection());
        if (!startupSelection.equals(container.getExtra("startupSelection"))) {
            WineUtils.changeServicesStatus(container, container.getStartupSelection() != Container.STARTUP_SELECTION_NORMAL);
            container.putExtra("startupSelection", startupSelection);
            containerDataChanged = true;
        }

        if (containerDataChanged) container.saveData();
    }

    private void setupXEnvironment() {
        envVars.put("MESA_DEBUG", "silent");
        envVars.put("MESA_NO_ERROR", "1");
        envVars.put("WINEPREFIX", ImageFs.WINEPREFIX);

        boolean enableWineDebug = preferences.getBoolean("enable_wine_debug", false);
        String wineDebugChannels = preferences.getString("wine_debug_channels", SettingsFragment.DEFAULT_WINE_DEBUG_CHANNELS);
        envVars.put("WINEDEBUG", enableWineDebug && !wineDebugChannels.isEmpty() ? "+"+wineDebugChannels.replace(",", ",+") : "-all");

        String rootPath = imageFs.getRootDir().getPath();
        FileUtils.clear(imageFs.getTmpDir());

        GuestProgramLauncherComponent guestProgramLauncherComponent = new GuestProgramLauncherComponent();

        if (container != null) {
            if (container.getStartupSelection() == Container.STARTUP_SELECTION_AGGRESSIVE) winHandler.killProcess("services.exe");

            boolean wow64Mode = container.isWoW64Mode();
            String guestExecutable = wineInfo.getExecutable(this, wow64Mode)+" explorer /desktop=shell,"+xServer.screenInfo+" "+getWineStartCommand();
            guestProgramLauncherComponent.setWoW64Mode(wow64Mode);
            guestProgramLauncherComponent.setGuestExecutable(guestExecutable);

            envVars.putAll(container.getEnvVars());
            if (shortcut != null) envVars.putAll(shortcut.getExtra("envVars"));
            if (!envVars.has("WINEESYNC")) envVars.put("WINEESYNC", "1");

            ArrayList<String> bindingPaths = new ArrayList<>();
            for (String[] drive : container.drivesIterator()) bindingPaths.add(drive[1]);
            guestProgramLauncherComponent.setBindingPaths(bindingPaths.toArray(new String[0]));
            guestProgramLauncherComponent.setBox86Preset(shortcut != null ? shortcut.getExtra("box86Preset", container.getBox86Preset()) : container.getBox86Preset());
            guestProgramLauncherComponent.setBox64Preset(shortcut != null ? shortcut.getExtra("box64Preset", container.getBox64Preset()) : container.getBox64Preset());
        }

        environment = new XEnvironment(this, imageFs);
        environment.addComponent(new SysVSharedMemoryComponent(xServer, UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.SYSVSHM_SERVER_PATH)));
        environment.addComponent(new XServerComponent(xServer, UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.XSERVER_PATH)));
        environment.addComponent(new NetworkInfoUpdateComponent());

        if (audioDriver.equals("alsa")) {
            envVars.put("ANDROID_ALSA_SERVER", UnixSocketConfig.ALSA_SERVER_PATH);
            envVars.put("ANDROID_ASERVER_USE_SHM", "true");
            environment.addComponent(new ALSAServerComponent(UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.ALSA_SERVER_PATH)));
        }
        else if (audioDriver.equals("pulseaudio")) {
            envVars.put("PULSE_SERVER", UnixSocketConfig.PULSE_SERVER_PATH);
            environment.addComponent(new PulseAudioComponent(UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.PULSE_SERVER_PATH)));
        }

        if (graphicsDriver.equals("virgl")) {
            environment.addComponent(new VirGLRendererComponent(xServer, UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.VIRGL_SERVER_PATH)));
        }

        guestProgramLauncherComponent.setEnvVars(envVars);
        guestProgramLauncherComponent.setTerminationCallback((status) -> exit());
        environment.addComponent(guestProgramLauncherComponent);

        if (isGenerateWineprefix()) generateWineprefix();
        environment.startEnvironmentComponents();

        winHandler.start();
        envVars.clear();
        dxwrapperConfig = null;
    }

    private void setupUI() {
        FrameLayout rootView = findViewById(R.id.FLXServerDisplay);
        xServerView = new XServerView(this, xServer);
        final GLRenderer renderer = xServerView.getRenderer();
        renderer.setCursorVisible(false);

        if (shortcut != null) {
            if (shortcut.getExtra("forceFullscreen", "0").equals("1")) renderer.setForceFullscreenWMClass(shortcut.wmClass);
            renderer.setUnviewableWMClasses("explorer.exe");
        }

        xServer.setRenderer(renderer);
        rootView.addView(xServerView);

        globalCursorSpeed = preferences.getFloat("cursor_speed", 1.0f);
        globalCursorDirect = preferences.getBoolean("cursor_direct", false);
        touchpadView = new TouchpadView(this, xServer);
        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setCursorDirect(globalCursorDirect);
        touchpadView.setFourFingersTapCallback(() -> {
            if (!drawerLayout.isDrawerOpen(GravityCompat.START)) drawerLayout.openDrawer(GravityCompat.START);
        });
        rootView.addView(touchpadView);

        inputControlsView = new InputControlsView(this);
        inputControlsView.setOverlayOpacity(preferences.getFloat("overlay_opacity", InputControlsView.DEFAULT_OVERLAY_OPACITY));
        inputControlsView.setTouchpadView(touchpadView);
        inputControlsView.setXServer(xServer);
        inputControlsView.setVisibility(View.GONE);
        rootView.addView(inputControlsView);

        if (container != null && container.isShowFPS()) {
            frameRating = new FrameRating(this);
            frameRating.setVisibility(View.GONE);
            rootView.addView(frameRating);
        }

        if (shortcut != null) {
            String controlsProfile = shortcut.getExtra("controlsProfile");
            if (!controlsProfile.isEmpty()) {
                ControlsProfile profile = inputControlsManager.getProfile(Integer.parseInt(controlsProfile));
                if (profile != null) showInputControls(profile);
            }
        }

        AppUtils.observeSoftKeyboardVisibility(drawerLayout, renderer::setScreenOffsetYRelativeToCursor);
    }

    private void showInputControlsDialog() {
        final ContentDialog dialog = new ContentDialog(this, R.layout.input_controls_dialog);
        dialog.setTitle(R.string.input_controls);
        dialog.setIcon(R.drawable.icon_input_controls);

        final Spinner sProfile = dialog.findViewById(R.id.SProfile);
        Runnable loadProfileSpinner = () -> {
            ArrayList<ControlsProfile> profiles = inputControlsManager.getProfiles(true);
            ArrayList<String> profileItems = new ArrayList<>();
            int selectedPosition = 0;
            profileItems.add("-- "+getString(R.string.disabled)+" --");
            for (int i = 0; i < profiles.size(); i++) {
                ControlsProfile profile = profiles.get(i);
                if (profile == inputControlsView.getProfile()) selectedPosition = i + 1;
                profileItems.add(profile.getName());
            }

            sProfile.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, profileItems));
            sProfile.setSelection(selectedPosition);
        };
        loadProfileSpinner.run();

        final CheckBox cbRelativeMouseMovement = dialog.findViewById(R.id.CBRelativeMouseMovement);
        cbRelativeMouseMovement.setChecked(xServer.isRelativeMouseMovement());

        final CheckBox cbShowTouchscreenControls = dialog.findViewById(R.id.CBShowTouchscreenControls);
        cbShowTouchscreenControls.setChecked(inputControlsView.isShowTouchscreenControls());

        dialog.findViewById(R.id.BTSettings).setOnClickListener((v) -> {
            int position = sProfile.getSelectedItemPosition();
            Intent intent = new Intent(this, MainActivity.class);
            intent.putExtra("edit_input_controls", true);
            intent.putExtra("selected_profile_id", position > 0 ? inputControlsManager.getProfiles().get(position - 1).id : 0);
            editInputControlsCallback = () -> {
                hideInputControls();
                inputControlsManager.loadProfiles(true);
                loadProfileSpinner.run();
            };
            startActivityForResult(intent, MainActivity.EDIT_INPUT_CONTROLS_REQUEST_CODE);
        });

        dialog.setOnConfirmCallback(() -> {
            xServer.setRelativeMouseMovement(cbRelativeMouseMovement.isChecked());
            inputControlsView.setShowTouchscreenControls(cbShowTouchscreenControls.isChecked());
            int position = sProfile.getSelectedItemPosition();
            if (position > 0) {
                showInputControls(inputControlsManager.getProfiles().get(position - 1));
            }
            else hideInputControls();
        });

        dialog.show();
    }

    private void showInputControls(ControlsProfile profile) {
        inputControlsView.setVisibility(View.VISIBLE);
        inputControlsView.requestFocus();
        inputControlsView.setProfile(profile);

        touchpadView.setSensitivity(profile.getCursorSpeed() * globalCursorSpeed);
        touchpadView.setCursorDirect(profile.getCursorDirect());
        touchpadView.setPointerButtonRightEnabled(false);

        inputControlsView.invalidate();
    }

    private void hideInputControls() {
        inputControlsView.setShowTouchscreenControls(true);
        inputControlsView.setVisibility(View.GONE);
        inputControlsView.setProfile(null);

        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setCursorDirect(globalCursorDirect);
        touchpadView.setPointerButtonLeftEnabled(true);
        touchpadView.setPointerButtonRightEnabled(true);

        inputControlsView.invalidate();
    }

    private void extractGraphicsDriverFiles() {
        String cacheId = graphicsDriver;
        if (graphicsDriver.equals("turnip")) {
            cacheId += "-"+DefaultVersion.TURNIP+"-"+DefaultVersion.ZINK;
        }
        else if (graphicsDriver.equals("virgl")) {
            cacheId += "-"+DefaultVersion.VIRGL;
        }

        boolean changed = !cacheId.equals(container.getExtra("graphicsDriver"));
        File rootDir = imageFs.getRootDir();

        if (changed) {
            FileUtils.delete(new File(imageFs.getLib32Dir(), "libvulkan_freedreno.so"));
            FileUtils.delete(new File(imageFs.getLib64Dir(), "libvulkan_freedreno.so"));
            FileUtils.delete(new File(imageFs.getLib32Dir(), "libGL.so.1.7.0"));
            FileUtils.delete(new File(imageFs.getLib64Dir(), "libGL.so.1.7.0"));
            container.putExtra("graphicsDriver", cacheId);
            container.saveData();
        }

        if (graphicsDriver.equals("turnip")) {
            if (dxwrapper.equals("dxvk")) {
                DXVKConfigDialog.setEnvVars(this, dxwrapperConfig, envVars);
            }
            else if (dxwrapper.equals("vkd3d")) envVars.put("VKD3D_FEATURE_LEVEL", "12_1");

            envVars.put("GALLIUM_DRIVER", "zink");
            envVars.put("TU_OVERRIDE_HEAP_SIZE", "4096");
            if (!envVars.has("MESA_VK_WSI_PRESENT_MODE")) envVars.put("MESA_VK_WSI_PRESENT_MODE", "mailbox");
            envVars.put("vblank_mode", "0");

            if (!GPUInformation.isAdreno6xx(this)) {
                EnvVars userEnvVars = new EnvVars(container.getEnvVars());
                String tuDebug = userEnvVars.get("TU_DEBUG");
                if (!tuDebug.contains("sysmem")) userEnvVars.put("TU_DEBUG", (!tuDebug.isEmpty() ? tuDebug+"," : "")+"sysmem");
                container.setEnvVars(userEnvVars.toString());
            }

            boolean useDRI3 = preferences.getBoolean("use_dri3", true);
            if (!useDRI3) {
                envVars.put("MESA_VK_WSI_PRESENT_MODE", "immediate");
                envVars.put("MESA_VK_WSI_DEBUG", "sw");
            }

            if (changed) {
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "graphics_driver/turnip-"+DefaultVersion.TURNIP+".tzst", rootDir);
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "graphics_driver/zink-"+DefaultVersion.ZINK+".tzst", rootDir);
            }
        }
        else if (graphicsDriver.equals("virgl")) {
            envVars.put("GALLIUM_DRIVER", "virpipe");
            envVars.put("VIRGL_NO_READBACK", "true");
            envVars.put("VIRGL_SERVER_PATH", UnixSocketConfig.VIRGL_SERVER_PATH);
            envVars.put("MESA_EXTENSION_OVERRIDE", "-GL_EXT_vertex_array_bgra");
            envVars.put("MESA_GL_VERSION_OVERRIDE", "3.1");
            envVars.put("vblank_mode", "0");
            if (changed) TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "graphics_driver/virgl-"+DefaultVersion.VIRGL+".tzst", rootDir);
        }
    }

    private void showTouchpadHelpDialog() {
        ContentDialog dialog = new ContentDialog(this, R.layout.touchpad_help_dialog);
        dialog.setTitle(R.string.touchpad_help);
        dialog.setIcon(R.drawable.icon_help);
        dialog.findViewById(R.id.BTCancel).setVisibility(View.GONE);
        dialog.show();
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        return !winHandler.onGenericMotionEvent(event) && !touchpadView.onExternalMouseEvent(event) && super.dispatchGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return (!inputControlsView.onKeyEvent(event) && !winHandler.onKeyEvent(event) && xServer.keyboard.onKeyEvent(event)) ||
               (!ExternalController.isGameController(event.getDevice()) && super.dispatchKeyEvent(event));
    }

    public InputControlsView getInputControlsView() {
        return inputControlsView;
    }

    private void generateWineprefix() {
        Intent intent = getIntent();

        final File rootDir = imageFs.getRootDir();
        final File installedWineDir = imageFs.getInstalledWineDir();
        wineInfo = intent.getParcelableExtra("wine_info");
        envVars.put("WINEARCH", wineInfo.isWin64() ? "win64" : "win32");
        imageFs.setWinePath(wineInfo.path);

        final File containerPatternDir = new File(installedWineDir, "/preinstall/container-pattern");
        if (containerPatternDir.isDirectory()) FileUtils.delete(containerPatternDir);
        containerPatternDir.mkdirs();

        File linkFile = new File(rootDir, ImageFs.HOME_PATH);
        linkFile.delete();
        FileUtils.symlink(".."+FileUtils.toRelativePath(rootDir.getPath(), containerPatternDir.getPath()), linkFile.getPath());

        GuestProgramLauncherComponent guestProgramLauncherComponent = environment.getComponent(GuestProgramLauncherComponent.class);
        guestProgramLauncherComponent.setGuestExecutable(wineInfo.getExecutable(this, false)+" explorer /desktop=shell,"+Container.DEFAULT_SCREEN_SIZE+" winecfg");

        final PreloaderDialog preloaderDialog = new PreloaderDialog(this);
        guestProgramLauncherComponent.setTerminationCallback((status) -> Executors.newSingleThreadExecutor().execute(() -> {
            if (status > 0) {
                AppUtils.showToast(this, R.string.unable_to_install_wine);
                FileUtils.delete(new File(installedWineDir, "/preinstall"));
                AppUtils.restartApplication(this);
                return;
            }

            preloaderDialog.showOnUiThread(R.string.finishing_installation);
            FileUtils.writeString(new File(rootDir, ImageFs.WINEPREFIX+"/.update-timestamp"), "disable\n");

            File userDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/users/xuser");
            File[] userFiles = userDir.listFiles();
            if (userFiles != null) {
                for (File userFile : userFiles) {
                    if (FileUtils.isSymlink(userFile)) {
                        String path = userFile.getPath();
                        userFile.delete();
                        (new File(path)).mkdirs();
                    }
                }
            }

            String suffix = wineInfo.fullVersion()+"-"+wineInfo.getArch();
            File containerPatternFile = new File(installedWineDir, "/preinstall/container-pattern-"+suffix+".tzst");
            TarCompressorUtils.compress(TarCompressorUtils.Type.ZSTD, new File(rootDir, ImageFs.WINEPREFIX), containerPatternFile, MainActivity.CONTAINER_PATTERN_COMPRESSION_LEVEL);

            if (!containerPatternFile.renameTo(new File(installedWineDir, containerPatternFile.getName())) ||
                !(new File(wineInfo.path)).renameTo(new File(installedWineDir, wineInfo.identifier()))) {
                containerPatternFile.delete();
            }

            FileUtils.delete(new File(installedWineDir, "/preinstall"));

            preloaderDialog.closeOnUiThread();
            AppUtils.restartApplication(this, R.id.main_menu_settings);
        }));
    }

    private void extractDXWrapperFiles(String dxwrapper) {
        final String[] dlls = {"d3d10.dll", "d3d10_1.dll", "d3d10core.dll", "d3d11.dll", "d3d12.dll", "d3d12core.dll", "d3d8.dll", "d3d9.dll", "dxgi.dll", "ddraw.dll"};
        if (firstTimeBoot && !dxwrapper.equals("vkd3d")) cloneOriginalDllFiles(dlls);
        File rootDir = imageFs.getRootDir();
        File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");

        switch (dxwrapper) {
            case "wined3d":
                restoreOriginalDllFiles(dlls);
                break;
            case "cnc-ddraw":
                restoreOriginalDllFiles(dlls);
                final String assetDir = "dxwrapper/cnc-ddraw-"+DefaultVersion.CNC_DDRAW;
                File configFile = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/ProgramData/cnc-ddraw/ddraw.ini");
                if (!configFile.isFile()) FileUtils.copy(this, assetDir+"/ddraw.ini", configFile);
                File shadersDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/ProgramData/cnc-ddraw/Shaders");
                FileUtils.delete(shadersDir);
                FileUtils.copy(this, assetDir+"/Shaders", shadersDir);
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, assetDir+"/ddraw.tzst", windowsDir, onExtractFileListener);
                break;
            case "vkd3d":
                String[] dxvkVersions = getResources().getStringArray(R.array.dxvk_version_entries);
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "dxwrapper/dxvk-"+(dxvkVersions[dxvkVersions.length-1])+".tzst", windowsDir, onExtractFileListener);
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "dxwrapper/vkd3d-"+DefaultVersion.VKD3D+".tzst", windowsDir, onExtractFileListener);
                break;
            default:
                restoreOriginalDllFiles("d3d12.dll", "d3d12core.dll", "ddraw.dll");
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "dxwrapper/"+dxwrapper+".tzst", windowsDir, onExtractFileListener);
                TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "dxwrapper/d8vk-"+DefaultVersion.D8VK+".tzst", windowsDir, onExtractFileListener);
                break;
        }
    }

    private void extractWinComponentFiles() {
        File rootDir = imageFs.getRootDir();
        File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");
        File systemRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/system.reg");

        try {
            JSONObject wincomponentsJSONObject = new JSONObject(FileUtils.readString(this, "wincomponents/wincomponents.json"));
            ArrayList<String> dlls = new ArrayList<>();
            String wincomponents = shortcut != null ? shortcut.getExtra("wincomponents", container.getWinComponents()) : container.getWinComponents();

            if (firstTimeBoot) {
                for (String[] wincomponent : new KeyValueSet(wincomponents)) {
                    JSONArray dlnames = wincomponentsJSONObject.getJSONArray(wincomponent[0]);
                    for (int i = 0; i < dlnames.length(); i++) {
                        String dlname = dlnames.getString(i);
                        dlls.add(!dlname.endsWith(".exe") ? dlname+".dll" : dlname);
                    }
                }

                cloneOriginalDllFiles(dlls.toArray(new String[0]));
                dlls.clear();
            }

            Iterator<String[]> oldWinComponentsIter = new KeyValueSet(container.getExtra("wincomponents", Container.FALLBACK_WINCOMPONENTS)).iterator();

            for (String[] wincomponent : new KeyValueSet(wincomponents)) {
                if (wincomponent[1].equals(oldWinComponentsIter.next()[1])) continue;
                String identifier = wincomponent[0];
                boolean useNative = wincomponent[1].equals("1");

                if (useNative) {
                    TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "wincomponents/"+identifier+".tzst", windowsDir, onExtractFileListener);
                }
                else {
                    JSONArray dlnames = wincomponentsJSONObject.getJSONArray(identifier);
                    for (int i = 0; i < dlnames.length(); i++) {
                        String dlname = dlnames.getString(i);
                        dlls.add(!dlname.endsWith(".exe") ? dlname+".dll" : dlname);
                    }
                }

                WineUtils.setWinComponentRegistryKeys(systemRegFile, identifier, useNative);
            }

            if (!dlls.isEmpty()) restoreOriginalDllFiles(dlls.toArray(new String[0]));
            WineUtils.overrideWinComponentDlls(this, container, wincomponents);
        }
        catch (JSONException e) {}
    }

    private void restoreOriginalDllFiles(final String... dlls) {
        File rootDir = imageFs.getRootDir();
        File cacheDir = new File(rootDir, ImageFs.CACHE_PATH+"/original_dlls");
        if (cacheDir.isDirectory()) {
            File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");
            String[] dirnames = cacheDir.list();
            int filesCopied = 0;

            for (String dll : dlls) {
                boolean success = false;
                for (String dirname : dirnames) {
                    File srcFile = new File(cacheDir, dirname+"/"+dll);
                    File dstFile = new File(windowsDir, dirname+"/"+dll);
                    if (FileUtils.copy(srcFile, dstFile)) success = true;
                }
                if (success) filesCopied++;
            }

            if (filesCopied == dlls.length) return;
        }

        containerManager.extractContainerPatternFile(container.getWineVersion(), container.getRootDir(), (file, size) -> {
            String path = file.getPath();
            if (path.contains("system32/") || path.contains("syswow64/")) {
                for (String dll : dlls) {
                    if (path.endsWith("system32/"+dll) || path.endsWith("syswow64/"+dll)) return file;
                }
            }
            return null;
        });

        cloneOriginalDllFiles(dlls);
    }

    private void cloneOriginalDllFiles(final String... dlls) {
        File rootDir = imageFs.getRootDir();
        File cacheDir = new File(rootDir, ImageFs.CACHE_PATH+"/original_dlls");
        if (!cacheDir.isDirectory()) cacheDir.mkdirs();
        File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");
        String[] dirnames = {"system32", "syswow64"};

        for (String dll : dlls) {
            for (String dirname : dirnames) {
                File dllFile = new File(windowsDir, dirname+"/"+dll);
                if (dllFile.isFile()) FileUtils.copy(dllFile, new File(cacheDir, dirname+"/"+dll));
            }
        }
    }

    private boolean isGenerateWineprefix() {
        return getIntent().getBooleanExtra("generate_wineprefix", false);
    }

    private String getWineStartCommand() {
        File tempDir = new File(container.getRootDir(), ".wine/drive_c/windows/temp");
        FileUtils.clear(tempDir);

        String args = "";
        if (shortcut != null) {
            String execArgs = shortcut.getExtra("execArgs");
            execArgs = !execArgs.isEmpty() ? " "+execArgs : "";

            if (shortcut.path.endsWith(".lnk")) {
                args += "\""+shortcut.path+"\""+execArgs;
            }
            else {
                String exeDir = FileUtils.getDirname(shortcut.path);
                String filename = FileUtils.getName(shortcut.path);
                int dotIndex, spaceIndex;
                if ((dotIndex = filename.lastIndexOf(".")) != -1 && (spaceIndex = filename.indexOf(" ", dotIndex)) != -1) {
                    execArgs = filename.substring(spaceIndex+1)+execArgs;
                    filename = filename.substring(0, spaceIndex);
                }
                args += "/dir "+exeDir.replace(" ", "\\ ")+" \""+filename+"\""+execArgs;
            }
        }
        else args += "\"wfm.exe\"";

        return "winhandler.exe "+args;
    }

    public XServer getXServer() {
        return xServer;
    }

    public WinHandler getWinHandler() {
        return winHandler;
    }

    public XServerView getXServerView() {
        return xServerView;
    }

    public Container getContainer() {
        return container;
    }

    private void changeWineAudioDriver() {
        if (!audioDriver.equals(container.getExtra("audioDriver"))) {
            File rootDir = imageFs.getRootDir();
            File userRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/user.reg");
            try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
                if (audioDriver.equals("alsa")) {
                    registryEditor.setStringValue("Software\\Wine\\Drivers", "Audio", "alsa");
                }
                else if (audioDriver.equals("pulseaudio")) {
                    registryEditor.setStringValue("Software\\Wine\\Drivers", "Audio", "pulse");
                }
            }
            container.putExtra("audioDriver", audioDriver);
            container.saveData();
        }
    }

    private void applyGeneralPatches(Container container) {
        File rootDir = imageFs.getRootDir();
        FileUtils.delete(new File(rootDir, "/opt/apps"));
        TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "imagefs_patches.tzst", rootDir, onExtractFileListener);
        TarCompressorUtils.extract(TarCompressorUtils.Type.ZSTD, this, "pulseaudio.tzst", new File(getFilesDir(), "pulseaudio"));
        WineUtils.applySystemTweaks(this, wineInfo);
        container.putExtra("graphicsDriver", null);
        container.putExtra("desktopTheme", null);
        SettingsFragment.resetBox86_64Version(this);
    }

    private void assignTaskAffinity(Window window) {
        if (taskAffinityMask == 0) return;
        int processId = window.getProcessId();
        String className = window.getClassName();
        int processAffinity = window.isWoW64() ? taskAffinityMaskWoW64 : taskAffinityMask;

        if (processId > 0) {
            winHandler.setProcessAffinity(processId, processAffinity);
        }
        else if (!className.isEmpty()) {
            winHandler.setProcessAffinity(window.getClassName(), processAffinity);
        }
    }

    private void changeFrameRatingVisibility(Window window, Property property) {
        if (frameRating == null) return;
        if (property != null) {
            if (frameRatingWindowId == -1 && window.attributes.isMapped() && property.nameAsString().equals("_MESA_DRV")) {
                frameRatingWindowId = window.id;
            }
        }
        else if (window.id == frameRatingWindowId) {
            frameRatingWindowId = -1;
            runOnUiThread(() -> frameRating.setVisibility(View.GONE));
        }
    }
}