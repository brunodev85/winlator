package com.winlator.cmod;

import android.Manifest;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.Html;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ForegroundColorSpan;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceManager;

import com.google.android.material.navigation.NavigationView;
import com.winlator.cmod.R;
import com.winlator.cmod.contentdialog.ContentDialog;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.core.ImageUtils;
import com.winlator.cmod.core.PreloaderDialog;
import com.winlator.cmod.container.ContainerManager;
import com.winlator.cmod.core.WineThemeManager;
import com.winlator.cmod.xenvironment.ImageFsInstaller;

import java.io.File;
import java.util.List;

public class MainActivity extends AppCompatActivity implements NavigationView.OnNavigationItemSelectedListener {
    public static final @IntRange(from = 1, to = 19) byte CONTAINER_PATTERN_COMPRESSION_LEVEL = 9;
    public static final byte PERMISSION_WRITE_EXTERNAL_STORAGE_REQUEST_CODE = 1;
    public static final byte OPEN_FILE_REQUEST_CODE = 2;
    public static final byte EDIT_INPUT_CONTROLS_REQUEST_CODE = 3;
    public static final byte OPEN_DIRECTORY_REQUEST_CODE = 4;
    public static final byte OPEN_IMAGE_REQUEST_CODE = 5;
    private DrawerLayout drawerLayout;
    public final PreloaderDialog preloaderDialog = new PreloaderDialog(this);
    private boolean editInputControls = false;
    private int selectedProfileId;
    private SharedPreferences sharedPreferences;
    private ContainerManager containerManager;
    private boolean isDarkMode;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Get shared preferences
        SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);

        // Check if Big Picture Mode is enabled
        boolean isBigPictureModeEnabled = sharedPreferences.getBoolean("enable_big_picture_mode", false);

        if (isBigPictureModeEnabled) {
            // If enabled, launch the BigPictureActivity and finish MainActivity
            Intent intent = new Intent(MainActivity.this, BigPictureActivity.class);
            startActivity(intent);
        }

        // Load the user's preferred theme
        sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
        isDarkMode = sharedPreferences.getBoolean("dark_mode", false);

        // Apply the theme based on the preference
        if (isDarkMode) {
            setTheme(R.style.AppTheme_Dark);
        } else {
            setTheme(R.style.AppTheme);
        }


        setContentView(R.layout.main_activity);

        drawerLayout = findViewById(R.id.DrawerLayout);
        NavigationView navigationView = findViewById(R.id.NavigationView);
        navigationView.setNavigationItemSelectedListener(this);

        setSupportActionBar(findViewById(R.id.Toolbar));
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
            actionBar.setHomeAsUpIndicator(R.drawable.icon_action_bar_menu);
        }

        // Determine text color based on dark mode
        int textColor = isDarkMode ? Color.WHITE : Color.BLACK;
        setNavigationViewItemTextColor(navigationView, textColor);

        // Create Winlator folder if not present
        File winlatorDir = new File(SettingsFragment.DEFAULT_WINLATOR_PATH);
        if (!winlatorDir.exists())
            winlatorDir.mkdirs();

        containerManager = new ContainerManager(this);

        Intent intent = getIntent();
        editInputControls = intent.getBooleanExtra("edit_input_controls", false);
        if (editInputControls) {
            selectedProfileId = intent.getIntExtra("selected_profile_id", 0);
            actionBar.setHomeAsUpIndicator(R.drawable.icon_action_bar_back);
            onNavigationItemSelected(navigationView.getMenu().findItem(R.id.main_menu_input_controls));
            navigationView.setCheckedItem(R.id.main_menu_input_controls);
        } else {
            int selectedMenuItemId = intent.getIntExtra("selected_menu_item_id", 0);
            int menuItemId = selectedMenuItemId > 0 ? selectedMenuItemId : R.id.main_menu_containers;

            actionBar.setHomeAsUpIndicator(R.drawable.icon_action_bar_menu);
            onNavigationItemSelected(navigationView.getMenu().findItem(menuItemId));
            navigationView.setCheckedItem(menuItemId);

            if (!requestAppPermissions()) {
                ImageFsInstaller.installIfNeeded(this);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
                showAllFilesAccessDialog();
            }

            if (Build.VERSION.SDK_INT >= 33) {
                if (ActivityCompat.checkSelfPermission(this, android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 0);
                }
            }
        }
    }

    private void showAllFilesAccessDialog() {
        new AlertDialog.Builder(this)
                .setTitle("All Files Access Required")
                .setMessage("In order to grant access to additional storage devices such as USB storage device, the All Files Access permission must be granted. Press Okay to grant All Files Access in your Android Settings.")
                .setPositiveButton("Okay", (dialog, which) -> {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_WRITE_EXTERNAL_STORAGE_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                ImageFsInstaller.installIfNeeded(this);
            }
            else finish();
        }
    }

    @Override
    public void onBackPressed() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        List<Fragment> fragments = fragmentManager.getFragments();
        for (Fragment fragment : fragments) {
            if (fragment instanceof ContainersFragment && fragment.isVisible()) {
                finish();
                return;
            }
        }
        if (!editInputControls)
            show(new ContainersFragment(), true);  // Pass `true` to trigger the reverse animation
        else
            super.onBackPressed();
    }

    private boolean requestAppPermissions() {
        boolean hasWritePermission = ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        boolean hasReadPermission = ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        boolean hasManageStoragePermission = Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager();

        if (hasWritePermission && hasReadPermission && hasManageStoragePermission) {
            return false; // All permissions are granted
        }

        if (!hasWritePermission || !hasReadPermission) {
            String[] permissions = new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE};
            ActivityCompat.requestPermissions(this, permissions, PERMISSION_WRITE_EXTERNAL_STORAGE_REQUEST_CODE);
        }

        return true; // Permissions are still being requested
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem menuItem) {
        if (menuItem.getItemId() == android.R.id.home) {
            if (editInputControls) {
                onBackPressed();
                return true;
            }

            if (drawerLayout.isDrawerOpen(GravityCompat.START)) {
                drawerLayout.closeDrawer(GravityCompat.START);
            } else {
                drawerLayout.openDrawer(GravityCompat.START);
            }
            return true;
        } else {
            return super.onOptionsItemSelected(menuItem);
        }
    }

    public void toggleDrawer() {
        if (drawerLayout.isDrawerOpen(GravityCompat.START)) {
            drawerLayout.closeDrawer(GravityCompat.START);
        } else {
            drawerLayout.openDrawer(GravityCompat.START);
        }
    }

    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (fragmentManager.getBackStackEntryCount() > 0) {
            fragmentManager.popBackStack(null, FragmentManager.POP_BACK_STACK_INCLUSIVE);
        }

        switch (item.getItemId()) {
            case R.id.main_menu_shortcuts:
                show(new ShortcutsFragment(), false);  // Forward animation
                break;
            case R.id.main_menu_containers:
                show(new ContainersFragment(), false);  // Forward animation
                break;
            case R.id.main_menu_input_controls:
                show(new InputControlsFragment(selectedProfileId), false);  // Forward animation
                break;
            case R.id.main_menu_contents:
                show(new ContentsFragment(), false);  // Forward animation
                break;
            case R.id.main_menu_adrenotools_gpu_drivers:
                show(new AdrenotoolsFragment(), false);
                break;
            case R.id.main_menu_settings:
                show(new SettingsFragment(), false);  // Forward animation
                break;
            case R.id.main_menu_about:
                showAboutDialog();
                break;
        }
        return true;
    }


//    private void show(Fragment fragment) {
//        FragmentManager fragmentManager = getSupportFragmentManager();
//        fragmentManager.beginTransaction()
//                .replace(R.id.FLFragmentContainer, fragment)
//                .commit();
//
//        drawerLayout.closeDrawer(GravityCompat.START);
//    }

    private void show(Fragment fragment, boolean reverse) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (reverse) {
            fragmentManager.beginTransaction()
                    .setCustomAnimations(R.anim.slide_in_down, R.anim.slide_out_up)  // Reverse animation
                    .replace(R.id.FLFragmentContainer, fragment)
                    .commit();
        } else {
            fragmentManager.beginTransaction()
                    .setCustomAnimations(R.anim.slide_in_up, R.anim.slide_out_down)  // Forward animation
                    .replace(R.id.FLFragmentContainer, fragment)
                    .commit();
        }

        drawerLayout.closeDrawer(GravityCompat.START);
    }

    private void showAboutDialog() {
        ContentDialog dialog = new ContentDialog(this, R.layout.about_dialog);
        dialog.findViewById(R.id.LLBottomBar).setVisibility(View.GONE);

        if (isDarkMode) {
            dialog.getWindow().setBackgroundDrawableResource(R.drawable.content_dialog_background_dark);
        } else {
            dialog.getWindow().setBackgroundDrawableResource(R.drawable.content_dialog_background);
        }

        try {
            final PackageInfo pInfo = getPackageManager().getPackageInfo(getPackageName(), 0);

            TextView tvWebpage = dialog.findViewById(R.id.TVWebpage);
            tvWebpage.setText(Html.fromHtml("<a href=\"https://www.winlator.org\">winlator.org</a>", Html.FROM_HTML_MODE_LEGACY));
            tvWebpage.setMovementMethod(LinkMovementMethod.getInstance());

            ((TextView) dialog.findViewById(R.id.TVAppVersion)).setText(getString(R.string.version) + " " + pInfo.versionName);

            String creditsAndThirdPartyAppsHTML = String.join("<br />",
                    "Winlator Cmod by coffincolors, me (<a href=\"https://github.com/coffincolors/winlator\">Fork</a>, <a href=\"https://github.com/Pipetto-crypto/winlator\">Fork</a>)",
                    "Big Picture Mode Music by",
                    "Dale Melvin Blevens III (Fumer)",
                    "---",
                    "Termux Package(<a href=\"https://github.com/termux/termux-packages\">github.com/termux/termux-package</a>)",
                    "Wine (<a href=\"https://www.winehq.org\">winehq.org</a>)",
                    "Box64 (<a href=\"https://github.com/ptitSeb/box64\">github.com/ptitSeb/box64</a>)",
                    "Mesa (Turnip/Zink/Wrapper) (<a href=\"https://github.com/xMeM/mesa/tree/wrapper\">github.com/xMeM/mesa</a>)",
                    "DXVK (<a href=\"https://github.com/doitsujin/dxvk\">github.com/doitsujin/dxvk</a>)",
                    "VKD3D (<a href=\"https://gitlab.winehq.org/wine/vkd3d\">gitlab.winehq.org/wine/vkd3d</a>)",
                    "D8VK (<a href=\"https://github.com/AlpyneDreams/d8vk\">github.com/AlpyneDreams/d8vk</a>)",
                    "CNC DDraw (<a href=\"https://github.com/FunkyFr3sh/cnc-ddraw\">github.com/FunkyFr3sh/cnc-ddraw</a>)",
                    "dxwrapper (<a href=\"https://github.com/elishacloud/dxwrapper\">github.com/elishacloud/dxwrapper</a>)",
                    "FEX-Emu (<a href=\"https://github.com/FEX-Emu/FEX\">github.com/FEX-Emu/FEX</a>)",
                    "libadrenotools (<a href=\"https://github.com/bylaws/libadrenotools\">github.com/bylaws/libadrenotools</a>)"
            );

            TextView tvCreditsAndThirdPartyApps = dialog.findViewById(R.id.TVCreditsAndThirdPartyApps);
            tvCreditsAndThirdPartyApps.setText(Html.fromHtml(creditsAndThirdPartyAppsHTML, Html.FROM_HTML_MODE_LEGACY));
            tvCreditsAndThirdPartyApps.setMovementMethod(LinkMovementMethod.getInstance());

            String glibcExpVersionForkHTML = String.join("<br />",
                    "longjunyu2's <a href=\"https://github.com/longjunyu2/winlator/tree/use-glibc-instead-of-proot\">(GLIBC Fork)</a>");
            TextView tvGlibcExpVersionFork = dialog.findViewById(R.id.TVGlibcExpVersionFork);
            tvGlibcExpVersionFork.setText(Html.fromHtml(glibcExpVersionForkHTML, Html.FROM_HTML_MODE_LEGACY));
            tvGlibcExpVersionFork.setMovementMethod(LinkMovementMethod.getInstance());
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        dialog.show();
    }

    private void setNavigationViewItemTextColor(NavigationView navigationView, int color) {
        for (int i = 0; i < navigationView.getMenu().size(); i++) {
            MenuItem menuItem = navigationView.getMenu().getItem(i);
            setMenuItemTextColor(menuItem, color);

            // If the menu item has sub-items, iterate through them
            if (menuItem.hasSubMenu()) {
                for (int j = 0; j < menuItem.getSubMenu().size(); j++) {
                    MenuItem subMenuItem = menuItem.getSubMenu().getItem(j);
                    setMenuItemTextColor(subMenuItem, color);
                }
            }
        }
    }

    private void setMenuItemTextColor(MenuItem menuItem, int color) {
        SpannableString spanString = new SpannableString(menuItem.getTitle());
        spanString.setSpan(new ForegroundColorSpan(color), 0, spanString.length(), 0);
        menuItem.setTitle(spanString);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == OPEN_IMAGE_REQUEST_CODE && resultCode == RESULT_OK) {
            Bitmap bitmap = ImageUtils.getBitmapFromUri(this, data.getData(), 1280);
            if (bitmap == null) return;
            File userWallpaperFile = WineThemeManager.getUserWallpaperFile(this);
            ImageUtils.save(bitmap, userWallpaperFile, Bitmap.CompressFormat.PNG, 100);
        }
    }
}
