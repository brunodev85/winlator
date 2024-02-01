package com.winlator;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.KeyEvent;
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
import com.winlator.core.AppUtils;
import com.winlator.core.CursorLocker;
import com.winlator.core.EnvVars;
import com.winlator.core.FileUtils;
import com.winlator.core.OBBImageInstaller;
import com.winlator.core.OnExtractFileListener;
import com.winlator.core.PreloaderDialog;
import com.winlator.core.ProcessHelper;
import com.winlator.core.TarZstdUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineRegistryEditor;
import com.winlator.core.WineStartMenuCreator;
import com.winlator.core.WineThemeManager;
import com.winlator.core.WineUtils;
import com.winlator.inputcontrols.ControlsProfile;
import com.winlator.inputcontrols.ExternalController;
import com.winlator.inputcontrols.InputControlsManager;
import com.winlator.renderer.GLRenderer;
import com.winlator.contentdialog.ContentDialog;
import com.winlator.widget.InputControlsView;
import com.winlator.widget.TouchpadView;
import com.winlator.widget.XServerView;
import com.winlator.winhandler.TaskManagerDialog;
import com.winlator.winhandler.WinHandler;
import com.winlator.xconnector.UnixSocketConfig;
import com.winlator.xenvironment.ImageFs;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xenvironment.components.ALSAServerComponent;
import com.winlator.xenvironment.components.EtcHostsFileUpdateComponent;
import com.winlator.xenvironment.components.GuestProgramLauncherComponent;
import com.winlator.xenvironment.components.PulseAudioComponent;
import com.winlator.xenvironment.components.SysVSharedMemoryComponent;
import com.winlator.xenvironment.components.VirGLRendererComponent;
import com.winlator.xenvironment.components.XServerComponent;
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
    private Runnable editInputControlsCallback;
    private Shortcut shortcut;
    private String graphicsDriver = Container.DEFAULT_GRAPHICS_DRIVER;
    private String audioDriver = Container.DEFAULT_AUDIO_DRIVER;
    private WineInfo wineInfo;
    private final EnvVars envVars = new EnvVars();
    private boolean firstTimeBoot = false;
    private SharedPreferences preferences;
    private OnExtractFileListener onExtractFileListener;
    private final WinHandler winHandler = new WinHandler(this);
    private float globalCursorSpeed = 1.0f;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        AppUtils.hideSystemUI(this);
        AppUtils.keepScreenOn(this);
        setContentView(R.layout.xserver_display_activity);

        final PreloaderDialog preloaderDialog = new PreloaderDialog(this);
        preloaderDialog.show(R.string.starting_up);

        preferences = PreferenceManager.getDefaultSharedPreferences(this);

        drawerLayout = findViewById(R.id.DrawerLayout);
        drawerLayout.setOnApplyWindowInsetsListener((view, windowInsets) -> windowInsets.replaceSystemWindowInsets(0, 0, 0, 0));
        drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED);

        NavigationView navigationView = findViewById(R.id.NavigationView);
        navigationView.setNavigationItemSelectedListener(this);

        imageFs = ImageFs.find(this);

        String screenSize = Container.DEFAULT_SCREEN_SIZE;
        if (!isGenerateWineprefix()) {
            containerManager = new ContainerManager(this);
            container = containerManager.getContainerById(getIntent().getIntExtra("container_id", 0));
            containerManager.activateContainer(container);

            firstTimeBoot = container.getExtra("appVersion").isEmpty();

            String wineVersion = container.getWineVersion();
            wineInfo = WineInfo.fromIdentifier(this, wineVersion);
            if (wineInfo != WineInfo.MAIN_WINE_VERSION) imageFs.setWinePath(wineInfo.path);

            String shortcutPath = getIntent().getStringExtra("shortcut_path");
            if (shortcutPath != null && !shortcutPath.isEmpty()) shortcut = new Shortcut(container, new File(shortcutPath));

            graphicsDriver = container.getGraphicsDriver();
            audioDriver = container.getAudioDriver();
            screenSize = container.getScreenSize();

            if (shortcut != null) {
                graphicsDriver = shortcut.getExtra("graphicsDriver", container.getGraphicsDriver());
                audioDriver = shortcut.getExtra("audioDriver", container.getAudioDriver());
                screenSize = shortcut.getExtra("screenSize", container.getScreenSize());

                String dinputMapperType = shortcut.getExtra("dinputMapperType");
                if (!dinputMapperType.isEmpty()) winHandler.setDInputMapperType(Byte.parseByte(dinputMapperType));
            }

            if (!wineInfo.isWin64()) {
                onExtractFileListener = (destination, entryName) -> {
                    if (entryName.contains("system32")) return null;
                    return new File(destination, entryName.replace("syswow64", "system32"));
                };
            }
        }

        inputControlsManager = new InputControlsManager(this);
        xServer = new XServer(new ScreenInfo(screenSize));
        xServer.setWinHandler(winHandler);
        xServer.windowManager.addOnWindowModificationListener(new WindowManager.OnWindowModificationListener() {
            @Override
            public void onUpdateWindowContent(Window window) {
                if (window.getWidth() > 1) {
                    xServerView.getRenderer().setCursorVisible(true);
                    preloaderDialog.closeOnUiThread();
                    xServer.windowManager.removeOnWindowModificationListener(this);
                }
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
        xServerView.onResume();
        if (environment != null) environment.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        if (environment != null) environment.onPause();
        xServerView.onPause();
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
                xServerView.getRenderer().toggleFullscreen();
                drawerLayout.closeDrawers();
                break;
            case R.id.main_menu_task_manager:
                (new TaskManagerDialog(this)).show();
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
        File rootDir = imageFs.getRootDir();
        String appVersion = String.valueOf(AppUtils.getVersionCode(this));
        String imgVersion = String.valueOf(imageFs.getVersion());
        boolean containerDataChanged = false;

        if (!container.getExtra("appVersion").equals(appVersion) || !container.getExtra("imgVersion").equals(imgVersion)) {
            File pulseaudioDir = new File(getFilesDir(), "pulseaudio");
            TarZstdUtils.extract(this, "patches.tzst", rootDir, onExtractFileListener);
            TarZstdUtils.extract(this, "pulseaudio.tzst", pulseaudioDir);
            WineUtils.applyRegistryTweaks(this);
            container.putExtra("appVersion", appVersion);
            container.putExtra("imgVersion", imgVersion);
            SettingsFragment.resetBox86_64Version(this);
            containerDataChanged = true;
        }

        String dxwrapper = shortcut != null ? shortcut.getExtra("dxwrapper", container.getDXWrapper()) : container.getDXWrapper();
        if (!dxwrapper.equals(container.getExtra("dxwrapper"))) {
            extractDXWrapperFiles(dxwrapper);
            container.putExtra("dxwrapper", dxwrapper);
            containerDataChanged = true;
        }
        if (dxwrapper.equals("cnc-ddraw")) envVars.put("CNC_DDRAW_CONFIG_FILE", "C:\\ProgramData\\cnc-ddraw\\ddraw.ini");

        String dxcomponents = shortcut != null ? shortcut.getExtra("dxcomponents", container.getDXComponents()) : container.getDXComponents();
        if (!dxcomponents.equals(container.getExtra("dxcomponents"))) {
            extractDXComponentFiles();
            container.putExtra("dxcomponents", dxcomponents);
            containerDataChanged = true;
        }

        String desktopTheme = container.getDesktopTheme();
        if (!desktopTheme.equals(container.getExtra("desktopTheme"))) {
            String[] desktopThemeArray = desktopTheme.split(",");
            WineThemeManager.apply(this, WineThemeManager.Theme.valueOf(desktopThemeArray[0]), desktopThemeArray[1]);
            container.putExtra("desktopTheme", desktopTheme);
        }

        if (containerDataChanged) container.saveData();
        WineStartMenuCreator.create(this, container);

        WineUtils.createDosdevicesSymlinks(container);
    }

    private void setupXEnvironment() {
        envVars.put("MESA_DEBUG", "silent");
        envVars.put("MESA_NO_ERROR", "1");
        envVars.put("WINEPREFIX", ImageFs.WINEPREFIX);
        envVars.put("WINEESYNC", "1");
        if (MainActivity.DEBUG_LEVEL <= 1) envVars.put("WINEDEBUG", "-all");

        String rootPath = imageFs.getRootDir().getPath();
        FileUtils.clear(imageFs.getTmpDir());

        GuestProgramLauncherComponent guestProgramLauncherComponent = new GuestProgramLauncherComponent();

        if (container != null) {
            if (container.isStopServicesOnStartup()) winHandler.killProcess("services.exe");

            String wineLoader = wineInfo.isWin64() ? "wine64" : "wine";
            String guestExecutable = wineLoader+" explorer /desktop=shell,"+xServer.screenInfo+" "+getWineStartCommand();
            guestProgramLauncherComponent.setGuestExecutable(guestExecutable);

            if (container.isShowFPS()) {
                envVars.put("GALLIUM_HUD", "simple,fps");
                envVars.put("DXVK_HUD", "fps");
            }
            envVars.putAll(container.getEnvVars());
            if (shortcut != null) envVars.putAll(shortcut.getExtra("envVars"));

            ArrayList<String> bindingPaths = new ArrayList<>();
            for (String[] drive : container.drivesIterator()) bindingPaths.add(drive[1]);
            guestProgramLauncherComponent.setBindingPaths(bindingPaths.toArray(new String[0]));
            guestProgramLauncherComponent.setCpuList(container.getCPUList());
            guestProgramLauncherComponent.setBox86Preset(shortcut != null ? shortcut.getExtra("box86Preset", container.getBox86Preset()) : container.getBox86Preset());
            guestProgramLauncherComponent.setBox64Preset(shortcut != null ? shortcut.getExtra("box64Preset", container.getBox64Preset()) : container.getBox64Preset());
        }

        environment = new XEnvironment(this, imageFs);
        environment.addComponent(new SysVSharedMemoryComponent(xServer, UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.SYSVSHM_SERVER_PATH)));
        environment.addComponent(new XServerComponent(xServer, UnixSocketConfig.createSocket(rootPath, UnixSocketConfig.XSERVER_PATH)));
        environment.addComponent(new EtcHostsFileUpdateComponent());

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
    }

    private void setupUI() {
        FrameLayout container = findViewById(R.id.FLXServerDisplay);
        xServerView = new XServerView(this, xServer);
        final GLRenderer renderer = xServerView.getRenderer();
        renderer.setCursorVisible(false);

        if (shortcut != null) {
            if (shortcut.getExtra("forceFullscreen", "0").equals("1")) renderer.setForceFullscreenWMClass(shortcut.wmClass);
            renderer.setUnviewableWMClasses("explorer.exe");
        }

        xServer.setRenderer(renderer);
        container.addView(xServerView);

        globalCursorSpeed = preferences.getFloat("cursor_speed", 1.0f);
        touchpadView = new TouchpadView(this, xServer);
        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setFourFingersTapCallback(() -> {
            if (!drawerLayout.isDrawerOpen(GravityCompat.START)) drawerLayout.openDrawer(GravityCompat.START);
        });
        container.addView(touchpadView);

        inputControlsView = new InputControlsView(this);
        inputControlsView.setOverlayOpacity(preferences.getFloat("overlay_opacity", InputControlsView.DEFAULT_OVERLAY_OPACITY));
        inputControlsView.setTouchpadView(touchpadView);
        inputControlsView.setXServer(xServer);
        inputControlsView.setVisibility(View.GONE);
        container.addView(inputControlsView);

        AppUtils.observeSoftKeyboardVisibility(drawerLayout, renderer::setScreenOffsetYRelativeToCursor);
    }

    private void showInputControlsDialog() {
        final ContentDialog dialog = new ContentDialog(this, R.layout.input_controls_dialog);
        dialog.setTitle(R.string.input_controls);
        dialog.setIcon(R.drawable.icon_input_controls);

        final Spinner sProfile = dialog.findViewById(R.id.SProfile);
        Runnable loadProfileSpinner = () -> {
            ArrayList<ControlsProfile> profiles = inputControlsManager.getProfiles();
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

        final CheckBox cbLockCursor = dialog.findViewById(R.id.CBLockCursor);
        cbLockCursor.setChecked(xServer.cursorLocker.getState() == CursorLocker.State.LOCKED);

        final CheckBox cbShowTouchscreenControls = dialog.findViewById(R.id.CBShowTouchscreenControls);
        cbShowTouchscreenControls.setChecked(inputControlsView.isShowTouchscreenControls());

        dialog.findViewById(R.id.BTSettings).setOnClickListener((v) -> {
            int position = sProfile.getSelectedItemPosition();
            Intent intent = new Intent(this, MainActivity.class);
            intent.putExtra("edit_input_controls", true);
            intent.putExtra("selected_profile_id", position > 0 ? inputControlsManager.getProfiles().get(position - 1).id : 0);
            editInputControlsCallback = () -> {
                hideInputControls();
                inputControlsManager.loadProfiles();
                loadProfileSpinner.run();
            };
            startActivityForResult(intent, MainActivity.EDIT_INPUT_CONTROLS_REQUEST_CODE);
        });

        dialog.setOnConfirmCallback(() -> {
            xServer.cursorLocker.setState(cbLockCursor.isChecked() ? CursorLocker.State.LOCKED : CursorLocker.State.CONFINED);
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
        touchpadView.setPointerButtonRightEnabled(false);

        inputControlsView.invalidate();
    }

    private void hideInputControls() {
        inputControlsView.setShowTouchscreenControls(true);
        inputControlsView.setVisibility(View.GONE);
        inputControlsView.setProfile(null);

        touchpadView.setSensitivity(globalCursorSpeed);
        touchpadView.setPointerButtonLeftEnabled(true);
        touchpadView.setPointerButtonRightEnabled(true);

        inputControlsView.invalidate();
    }

    private void extractGraphicsDriverFiles() {
        File rootDir = imageFs.getRootDir();

        FileUtils.delete(new File(rootDir, "/usr/lib/arm-linux-gnueabihf/libvulkan_freedreno.so"));
        FileUtils.delete(new File(rootDir, "/usr/lib/aarch64-linux-gnu/libvulkan_freedreno.so"));
        FileUtils.delete(new File(rootDir, "/usr/lib/arm-linux-gnueabihf/libGL.so.1.7.0"));
        FileUtils.delete(new File(rootDir, "/usr/lib/aarch64-linux-gnu/libGL.so.1.7.0"));

        switch (graphicsDriver) {
            case "llvmpipe":
                envVars.put("GALLIUM_DRIVER", "llvmpipe");
                TarZstdUtils.extract(this, "graphics_driver/llvmpipe-23.1.6.tzst", rootDir);
                break;
            case "turnip-zink":
                envVars.put("GALLIUM_DRIVER", "zink");
                envVars.put("DXVK_STATE_CACHE_PATH", ImageFs.CACHE_PATH);
                envVars.put("DXVK_LOG_LEVEL", "none");
                envVars.put("TU_DEBUG", "noconform");
                envVars.put("MESA_VK_WSI_PRESENT_MODE", "mailbox");
                envVars.put("vblank_mode", "0");

                boolean useDRI3 = preferences.getBoolean("use_dri3", true);
                if (!useDRI3) {
                    envVars.put("MESA_VK_WSI_PRESENT_MODE", "immediate");
                    envVars.put("MESA_VK_WSI_DEBUG", "sw");
                }

                String turnipVersion = preferences.getString("turnip_version", SettingsFragment.getDefaultTurnipVersion(this));
                TarZstdUtils.extract(this, "graphics_driver/turnip-"+turnipVersion+".tzst", rootDir);
                TarZstdUtils.extract(this, "graphics_driver/zink-22.2.2.tzst", rootDir);
                break;
            case "virgl":
                envVars.put("GALLIUM_DRIVER", "virpipe");
                envVars.put("VIRGL_NO_READBACK", "true");
                envVars.put("VIRGL_SERVER_PATH", UnixSocketConfig.VIRGL_SERVER_PATH);
                envVars.put("MESA_EXTENSION_OVERRIDE", "-GL_EXT_vertex_array_bgra -GL_EXT_texture_sRGB_decode -GL_ARB_ES2_compatibility");
                envVars.put("MESA_GL_VERSION_OVERRIDE", "3.1");
                envVars.put("vblank_mode", "0");
                TarZstdUtils.extract(this, "graphics_driver/virgl-22.1.7.tzst", rootDir);
                break;
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
        return !winHandler.onGenericMotionEvent(event) && super.dispatchGenericMotionEvent(event);
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

        String wineLoader = wineInfo.isWin64() ? "wine64" : "wine";
        GuestProgramLauncherComponent guestProgramLauncherComponent = environment.getComponent(GuestProgramLauncherComponent.class);
        guestProgramLauncherComponent.setGuestExecutable(wineLoader+" explorer /desktop=shell,"+Container.DEFAULT_SCREEN_SIZE+" winecfg");

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
            TarZstdUtils.compress(new File(rootDir, ImageFs.WINEPREFIX), containerPatternFile, MainActivity.CONTAINER_PATTERN_COMPRESSION_LEVEL);

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
        final String[] dlls = {"d3d10.dll", "d3d10_1.dll", "d3d10core.dll", "d3d11.dll", "d3d8.dll", "d3d9.dll", "dxgi.dll", "ddraw.dll", "wined3d.dll"};
        if (firstTimeBoot) cloneOriginalDllFiles(dlls);
        File rootDir = imageFs.getRootDir();
        File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");

        if (dxwrapper.equals("original-wined3d")) {
            restoreOriginalDllFiles(dlls);
        }
        else if (dxwrapper.equals("cnc-ddraw")) {
            restoreOriginalDllFiles(dlls);
            File configFile = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/ProgramData/cnc-ddraw/ddraw.ini");
            if (!configFile.isFile()) FileUtils.copy(this, "dxwrapper/cnc-ddraw/ddraw.ini", configFile);
            File shadersDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/ProgramData/cnc-ddraw/Shaders");
            if (!shadersDir.isDirectory()) FileUtils.copy(this, "dxwrapper/cnc-ddraw/Shaders", shadersDir);
            TarZstdUtils.extract(this, "dxwrapper/cnc-ddraw/ddraw.tzst", windowsDir, onExtractFileListener);
        }
        else {
            restoreOriginalDllFiles("ddraw.dll");
            TarZstdUtils.extract(this, "dxwrapper/"+dxwrapper+".tzst", windowsDir, onExtractFileListener);
        }
    }

    private void extractDXComponentFiles() {
        File rootDir = imageFs.getRootDir();
        File dxcomponentsDir = new File(rootDir, "/opt/resources/dxcomponents");
        File windowsDir = new File(rootDir, ImageFs.WINEPREFIX+"/drive_c/windows");
        File systemRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/system.reg");

        try {
            JSONObject dxcomponentsJSONObject = new JSONObject(FileUtils.readString(this, "dxcomponents.json"));
            ArrayList<String> dlls = new ArrayList<>();
            String dxcomponents = shortcut != null ? shortcut.getExtra("dxcomponents", container.getDXComponents()) : container.getDXComponents();

            if (firstTimeBoot) {
                for (String[] dxcomponent : Container.dxcomponentsIterator(dxcomponents)) {
                    JSONArray libNames = dxcomponentsJSONObject.getJSONArray(dxcomponent[0]);
                    for (int i = 0; i < libNames.length(); i++) {
                        String libName = libNames.getString(i);
                        dlls.add(!libName.endsWith(".exe") ? libName+".dll" : libName);
                    }
                }

                cloneOriginalDllFiles(dlls.toArray(new String[0]));
                dlls.clear();
            }

            Iterator<String[]> oldDXComponentsIter = Container.dxcomponentsIterator(container.getExtra("dxcomponents", dxcomponents)).iterator();

            for (String[] dxcomponent : Container.dxcomponentsIterator(dxcomponents)) {
                if (dxcomponent[1].equals(oldDXComponentsIter.next()[1])) continue;
                String identifier = dxcomponent[0];
                boolean useNative = dxcomponent[1].equals("1");

                if (useNative) {
                    TarZstdUtils.extract(new File(dxcomponentsDir, identifier+".tzst"), windowsDir, onExtractFileListener);
                }
                else {
                    JSONArray libNames = dxcomponentsJSONObject.getJSONArray(identifier);
                    for (int i = 0; i < libNames.length(); i++) {
                        String libName = libNames.getString(i);
                        dlls.add(!libName.endsWith(".exe") ? libName+".dll" : libName);
                    }
                }

                if (identifier.equals("directsound")) {
                    try (WineRegistryEditor registryEditor = new WineRegistryEditor(systemRegFile)) {
                        final String key64 = "Software\\Classes\\CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance\\{E30629D1-27E5-11CE-875D-00608CB78066}";
                        final String key32 = "Software\\Classes\\Wow6432Node\\CLSID\\{083863F1-70DE-11D0-BD40-00A0C911CE86}\\Instance\\{E30629D1-27E5-11CE-875D-00608CB78066}";

                        if (useNative) {
                            registryEditor.setStringValue(key32, "CLSID", "{E30629D1-27E5-11CE-875D-00608CB78066}");
                            registryEditor.setHexValue(key32, "FilterData", "02000000000080000100000000000000307069330200000000000000010000000000000000000000307479330000000038000000480000006175647300001000800000aa00389b710100000000001000800000aa00389b71");
                            registryEditor.setStringValue(key32, "FriendlyName", "Wave Audio Renderer");

                            registryEditor.setStringValue(key64, "CLSID", "{E30629D1-27E5-11CE-875D-00608CB78066}");
                            registryEditor.setHexValue(key64, "FilterData", "02000000000080000100000000000000307069330200000000000000010000000000000000000000307479330000000038000000480000006175647300001000800000aa00389b710100000000001000800000aa00389b71");
                            registryEditor.setStringValue(key64, "FriendlyName", "Wave Audio Renderer");
                        }
                        else {
                            registryEditor.removeKey(key32);
                            registryEditor.removeKey(key64);
                        }
                    }
                }
            }

            if (!dlls.isEmpty()) restoreOriginalDllFiles(dlls.toArray(new String[0]));
            WineUtils.overrideDXComponentDlls(this, container, dxcomponents);
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

        File containerPatternFile = containerManager.getContainerPatternFile(container.getWineVersion());
        TarZstdUtils.extract(containerPatternFile, container.getRootDir(), (destination, entryName) -> {
            if (entryName.contains("system32") || entryName.contains("syswow64")) {
                for (String dll : dlls) {
                    if (entryName.endsWith("system32/"+dll) || entryName.endsWith("syswow64/"+dll)) {
                        return new File(destination, entryName);
                    }
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
}