package com.winlator.cmod.contentdialog;

import android.content.Context;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.ToggleButton;

import com.winlator.cmod.R;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.contents.ContentProfile;
import com.winlator.cmod.contents.ContentsManager;
import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.DefaultVersion;
import com.winlator.cmod.core.EnvVars;
import com.winlator.cmod.core.KeyValueSet;
import com.winlator.cmod.core.StringUtils;
import com.winlator.cmod.core.VKD3DVersionItem;
import com.winlator.cmod.xenvironment.ImageFs;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DXVKConfigDialog extends ContentDialog {
    public static final String DEFAULT_CONFIG = Container.DEFAULT_DXWRAPPERCONFIG;
    public static final int DXVK_TYPE_NONE = 0;
    public static final int DXVK_TYPE_ASYNC = 1;
    public static final int DXVK_TYPE_GPLASYNC = 2;
    private final ToggleButton swAsync;
    private boolean isARM64EC = false;
    private final ToggleButton swAsyncCache;
    private final View llAsync;
    private final View llAsyncCache;
    private final Context context;
    private static List<String> dxvkVersions;
    private static final Pattern SEMVER = Pattern.compile("(\\d+)\\.(\\d+)(?:\\.(\\d+))?");

    private static Integer tryGetMajor(String s) {
        if (s == null) return null;
        Matcher m = SEMVER.matcher(s);
        if (!m.find()) return null;
        try {
            return Integer.parseInt(m.group(1));
        } catch (NumberFormatException e) {
            return null;
        }
    }
    
    public static final String[] VKD3D_FEATURE_LEVEL = {"12_0", "12_1", "12_2", "11_1", "11_0", "10_1", "10_0", "9_3", "9_2", "9_1"};

    private static int compareVersion(String varA, String varB) {
        final String[] levelsA = varA.split("\\.");
        final String[] levelsB = varB.split("\\.");
        int minLen = Math.min(levelsA.length, levelsB.length);
        int numA, numB;

        for (int i = 0; i < minLen; i++) {
            numA = Integer.parseInt(levelsA[i]);
            numB = Integer.parseInt(levelsB[i]);
            if (numA != numB)
                return numA - numB;
        }

        if (levelsA.length != levelsB.length)
            return levelsA.length - levelsB.length;

        return 0;
    }

    public DXVKConfigDialog(View anchor, boolean isARM64EC) {
        super(anchor.getContext(), R.layout.dxvk_config_dialog);
        context = anchor.getContext();
        setIcon(R.drawable.icon_settings);
        setTitle("DXVK "+context.getString(R.string.configuration));

        final Spinner sDXVKVersion = findViewById(R.id.SDXVKVersion);
        final Spinner sVKD3DVersion = findViewById(R.id.SVKD3DVersion);
        final Spinner sFramerate = findViewById(R.id.SFramerate);
        final Spinner sVKD3DFeatureLevel = findViewById(R.id.SVKD3DFeatureLevel);
        final Spinner sDDRAWrapper = findViewById(R.id.SDDRAWrapper);
        swAsync = findViewById(R.id.SWAsync);
        swAsyncCache = findViewById(R.id.SWAsyncCache);
        llAsync = findViewById(R.id.LLAsync);
        llAsyncCache = findViewById(R.id.LLAsyncCache);

        ContentsManager contentsManager = new ContentsManager(context);
        contentsManager.syncContents();

        KeyValueSet config = parseConfig(anchor.getTag());
        loadDxvkVersionSpinner(contentsManager, sDXVKVersion, isARM64EC);
        loadVkd3dVersionSpinner(contentsManager, sVKD3DVersion);

        ArrayAdapter<String> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_item, VKD3D_FEATURE_LEVEL);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        sVKD3DFeatureLevel.setAdapter(adapter);

        setDXVKSpinner(sDXVKVersion, config, contentsManager, isARM64EC);
        AppUtils.setSpinnerSelectionFromIdentifier(sFramerate, config.get("framerate"));
        AppUtils.setSpinnerSelectionFromIdentifier(sVKD3DVersion, config.get("vkd3dVersion"));
        AppUtils.setSpinnerSelectionFromIdentifier(sVKD3DFeatureLevel, config.get("vkd3dLevel"));
        AppUtils.setSpinnerSelectionFromIdentifier(sDDRAWrapper, config.get("ddrawrapper"));

        swAsync.setChecked(config.get("async").equals("1"));
        swAsyncCache.setChecked(config.get("asyncCache").equals("1"));

        updateConfigVisibility(getDXVKType(sDXVKVersion.getSelectedItemPosition()));

        sDXVKVersion.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                updateConfigVisibility(getDXVKType(position));
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {

            }
        });

        sVKD3DVersion.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String selectedVersion = sVKD3DVersion.getSelectedItem().toString();
                String currentDXVKVersion = config.get("version");

                if (!selectedVersion.equals("None")) {
                    ArrayList<String> versions = new ArrayList<>();

                    for (int i = 0; i < dxvkVersions.size(); i++) {
                        Integer major = tryGetMajor(dxvkVersions.get(i));
                        if (major != null && major < 2) {
                            versions.add(dxvkVersions.get(i));
                        }
                    }

                    dxvkVersions.removeAll(versions);

                    ArrayAdapter<String> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, dxvkVersions);
                    sDXVKVersion.setAdapter(adapter);

                    Integer curMajor = tryGetMajor(currentDXVKVersion);
                    AppUtils.setSpinnerSelectionFromIdentifier(
                            sDXVKVersion,
                            (curMajor != null && curMajor >= 2) ? currentDXVKVersion : DefaultVersion.DXVK
                    );
                    updateConfigVisibility(getDXVKType(sDXVKVersion.getSelectedItemPosition()));
                }
                else {
                    loadDxvkVersionSpinner(contentsManager, sDXVKVersion, isARM64EC);
                    AppUtils.setSpinnerSelectionFromIdentifier(sDXVKVersion, config.get("version"));
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        setOnConfirmCallback(() -> {
            config.put("version", sDXVKVersion.getSelectedItem().toString());
            config.put("framerate", StringUtils.parseNumber(sFramerate.getSelectedItem()));
            config.put("async", ((swAsync.isChecked())&&(llAsync.getVisibility()==View.VISIBLE))?"1":"0");
            config.put("asyncCache", ((swAsyncCache.isChecked())&&(llAsyncCache.getVisibility()==View.VISIBLE))?"1":"0");
            VKD3DVersionItem selectedItem = (VKD3DVersionItem) sVKD3DVersion.getSelectedItem();
            config.put("vkd3dVersion", selectedItem.getIdentifier());
            config.put("vkd3dLevel", sVKD3DFeatureLevel.getSelectedItem().toString());
            config.put("ddrawrapper", StringUtils.parseIdentifier(sDDRAWrapper.getSelectedItem().toString()));
            anchor.setTag(config.toString());
        });
    }

    private void updateConfigVisibility(int dxvkType) {
        if (dxvkType == DXVK_TYPE_ASYNC) {
            llAsync.setVisibility(View.VISIBLE);
            llAsyncCache.setVisibility(View.GONE);
        } else if (dxvkType == DXVK_TYPE_GPLASYNC) {
            llAsync.setVisibility(View.VISIBLE);
            llAsyncCache.setVisibility(View.VISIBLE);
        } else {
            llAsync.setVisibility(View.GONE);
            llAsyncCache.setVisibility(View.GONE);
        }
    }

    private int getDXVKType(int pos) {
        final String v = dxvkVersions.get(pos);
        int dxvkType = DXVK_TYPE_NONE;
        if (v.contains("gplasync"))
            dxvkType = DXVK_TYPE_GPLASYNC;
        else if (v.contains("async"))
            dxvkType = DXVK_TYPE_ASYNC;
        return dxvkType;
    }

    private void setDXVKSpinner(Spinner sDXVKVersion, KeyValueSet config, ContentsManager contentsManager, boolean isARM64EC) {
        String selectedVersion = config.get("vkd3dVersion");
        String currentDXVKVersion = config.get("version");
        if (!selectedVersion.equals("None")) {
            ArrayList<String> versions = new ArrayList<>();

            for (int i = 0; i < dxvkVersions.size(); i++) {
                Integer major = tryGetMajor(dxvkVersions.get(i));
                if (major != null && major < 2) {
                    versions.add(dxvkVersions.get(i));
                }
            }

            dxvkVersions.removeAll(versions);

            ArrayAdapter<String> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, dxvkVersions);
            sDXVKVersion.setAdapter(adapter);

            Integer curMajor = tryGetMajor(currentDXVKVersion);
            AppUtils.setSpinnerSelectionFromIdentifier(
                    sDXVKVersion,
                    (curMajor != null && curMajor >= 2) ? currentDXVKVersion : DefaultVersion.DXVK
            );
        }
        else
            AppUtils.setSpinnerSelectionFromIdentifier(sDXVKVersion, currentDXVKVersion);
    }

    public static KeyValueSet parseConfig(Object config) {
        String data = config != null && !config.toString().isEmpty() ? config.toString() :  DEFAULT_CONFIG;
        return new KeyValueSet(data);
    }

    public static void setEnvVars(Context context, KeyValueSet config, EnvVars envVars) {
        String content = "";

        String framerate = config.get("framerate");

        if (!framerate.isEmpty() && !framerate.equals("0")) {
            content += "dxgi.maxFrameRate = " + framerate + "; ";
            content += "d3d9.maxFrameRate = " + framerate;
            envVars.put("DXVK_FRAME_RATE", framerate);
        }

        String async = config.get("async");
        if (!async.isEmpty() && !async.equals("0"))
            envVars.put("DXVK_ASYNC", "1");

        String asyncCache = config.get("asyncCache");
        if (!asyncCache.isEmpty() && !asyncCache.equals("0"))
            envVars.put("DXVK_GPLASYNCCACHE", "1");

        if (!content.isEmpty())
            envVars.put("DXVK_CONFIG", content);

        envVars.put("VKD3D_FEATURE_LEVEL", config.get("vkd3dLevel"));
        envVars.put("DXVK_STATE_CACHE_PATH", context.getFilesDir() + "/imagefs/" + ImageFs.CACHE_PATH);
    }

    private void loadDxvkVersionSpinner(ContentsManager manager, Spinner spinner, boolean isARM64EC) {
        this.isARM64EC = isARM64EC;
        String[] originalItems = context.getResources().getStringArray(R.array.dxvk_version_entries);
        List<String> itemList = new ArrayList<>(Arrays.asList(originalItems));

        for (ContentProfile profile : manager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_DXVK)) {
            String entryName = ContentsManager.getEntryName(profile);
            int firstDashIndex = entryName.indexOf('-');
            itemList.add(entryName.substring(firstDashIndex + 1));
        }

        for (int i = 0; i < itemList.size(); i++) {
            if (itemList.get(i).contains("arm64ec") && !isARM64EC)
                itemList.remove(i);
        }

        spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, itemList));
        dxvkVersions = itemList;
    }

    private void loadVkd3dVersionSpinner(ContentsManager manager, Spinner spinner) {
        List<VKD3DVersionItem> itemList = new ArrayList<>();

        // Add predefined versions
        String[] originalItems = context.getResources().getStringArray(R.array.vkd3d_version_entries);
        for (String version : originalItems) {
            itemList.add(new VKD3DVersionItem(version)); // For predefined versions, use 0 as verCode
        }

        // Add installed content profiles
        for (ContentProfile profile : manager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_VKD3D)) {
            String displayName = profile.verName;  // Display name for the spinner
            int versionCode = profile.verCode;     // Unique version code if available
            itemList.add(new VKD3DVersionItem(displayName, versionCode));
        }

        ArrayAdapter<VKD3DVersionItem> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, itemList);
        spinner.setAdapter(adapter);
    }
}
