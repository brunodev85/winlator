package com.winlator.cmod;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.documentfile.provider.DocumentFile;
import androidx.preference.PreferenceManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.LinearSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.SnapHelper;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.winlator.cmod.R;
import com.winlator.cmod.bigpicture.BigPictureAdapter;
import com.winlator.cmod.bigpicture.CarouselItemDecoration;
import com.winlator.cmod.bigpicture.TiledBackgroundView;
import com.winlator.cmod.bigpicture.steamgrid.SteamGridDBApi;
import com.winlator.cmod.bigpicture.steamgrid.SteamGridGridsResponse;
import com.winlator.cmod.bigpicture.steamgrid.SteamGridGridsResponseDeserializer;
import com.winlator.cmod.bigpicture.steamgrid.SteamGridSearchResponse;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.container.ContainerManager;
import com.winlator.cmod.container.Shortcut;
import com.winlator.cmod.core.FileUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executors;

import okhttp3.OkHttpClient;
import retrofit2.Call;
import retrofit2.Callback;
import retrofit2.Response;
import retrofit2.Retrofit;
import retrofit2.converter.gson.GsonConverterFactory;

// For YouTube Playback
import android.webkit.WebView;
import android.webkit.WebViewClient;

import android.animation.ObjectAnimator;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.widget.Toast;

public class BigPictureActivity extends AppCompatActivity {
    private ImageView coverArtView;
    private TextView gameTitleView, graphicsDriverView, graphicsDriverVersionView, dxWrapperView, dxWrapperConfigView, audioDriverView, box64PresetView, playCountView, playtimeView;
    private RecyclerView recyclerView;
    private ContainerManager manager;
    private BigPictureAdapter adapter;
    private ImageButton playButton;

    private Shortcut currentShortcut;
    private int lastFocusedItemIndex = RecyclerView.NO_POSITION;

    private static String API_KEY = "0324c52513634547a7b32d6d323635d0";
    private static final String BASE_URL = "https://www.steamgriddb.com/api/v2/";

    private static final int REQUEST_CODE_UPLOAD_CUSTOM_COVER = 1069;
    private TextView uploadText; // Class-level variable

    private TextView emptyStateTextView;

    private WebView webView;

    private static final int REQUEST_CODE_SELECT_MP3 = 1070;
    private Uri selectedMp3Uri;

    private MediaPlayer mediaPlayer;

    // Declare constants for the request code and preferences key
    private static final int REQUEST_CODE_SELECT_WALLPAPER = 1080;
    private static final String WALLPAPER_PREF_KEY = "custom_wallpaper_path";
    private static final String WALLPAPER_DISPLAY_PREF_KEY = "wallpaper_display_mode";

    public static final String SEEK_BAR_PROGRESS_KEY = "frame_duration_seekbar";


    @Override
    protected void onStart() {
        super.onStart();
        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);
        backgroundView.startAnimation(); // Start animation
    }

    @Override
    protected void onStop() {
        super.onStop();
        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);
        backgroundView.stopAnimation(); // Stop animation
    }



    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getSupportActionBar().hide();  // Hide the action bar for full-screen mode
        setContentView(R.layout.big_picture_activity);

//        // Set the background to ImageView for the tiled animation background
//        ImageView parallaxBackgroundView = findViewById(R.id.parallaxBackgroundView);
//        parallaxBackgroundView.setBackgroundResource(R.drawable.animated_background); // Use animation as background
//
//        // Get the AnimationDrawable and start it directly
//        animatedBackground = (AnimationDrawable) parallaxBackgroundView.getBackground();
//        animatedBackground.setOneShot(false); // Loop the animation
//        parallaxBackgroundView.post(() -> animatedBackground.start());



        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);

        Button selectWallpaperButton = findViewById(R.id.selectWallpaperButton);
        RadioButton rbCustomWallpaper = findViewById(R.id.rbCustomWallpaper); // Get reference to the custom wallpaper button

        Button folderPickerButton = findViewById(R.id.selectPngFolderButton);
        folderPickerButton.setOnClickListener(v -> {
            selectPngFolder();
        });

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        String savedFolderUri = prefs.getString("png_folder_uri", null);
        if (savedFolderUri != null) {
            Uri folderUri = Uri.parse(savedFolderUri);
            loadFramesFromFolder(folderUri);
        }

        int storedFrameDuration = prefs.getInt("frame_duration", 66);
        TiledBackgroundView bgView = findViewById(R.id.parallaxBackgroundView);
        if (bgView != null) {
            bgView.setFrameDuration(storedFrameDuration);
        }


        // In onCreate, after inflating views:
        SeekBar frameSpeedSeekBar = findViewById(R.id.frameSpeedSeekBar);
        bgView = findViewById(R.id.parallaxBackgroundView);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);

// 1) Restore the previous SeekBar progress
        int storedSeekBarProgress = prefs.getInt(SEEK_BAR_PROGRESS_KEY, 66); // default 66
// 2) Set the SeekBar position
        frameSpeedSeekBar.setProgress(storedSeekBarProgress);

// 3) Convert that storedSeekBarProgress to actual frameDuration
        int reversedProgress = frameSpeedSeekBar.getMax() - storedSeekBarProgress;
        bgView.setFrameDuration(reversedProgress);

// Now attach the listener
        TiledBackgroundView finalBgView = bgView;
        SharedPreferences finalPrefs = prefs;
        frameSpeedSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                // Because you’re reversing the direction:
                int reversed = seekBar.getMax() - progress;
                // Apply to the TiledBackgroundView
                finalBgView.setFrameDuration(reversed);

                // Store the *raw* SeekBar progress so we can restore next time
                finalPrefs.edit().putInt(SEEK_BAR_PROGRESS_KEY, progress).apply();
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { }
        });


        RadioGroup animationSelectorGroup = findViewById(R.id.animationSelectorGroup);
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);

// Temporarily disable listener to avoid unintended triggers
        animationSelectorGroup.setOnCheckedChangeListener(null);

// Restore the selected animation state for the RadioGroup
        String savedAnimation = preferences.getString("selected_animation", "ab"); // Default to "ab" or your preferred default
        if (savedAnimation.equals("custom_wallpaper")) {
            ((RadioButton) findViewById(R.id.rbCustomWallpaper)).setChecked(true);
        } else if (savedAnimation.equals("ab_gear")) {
            ((RadioButton) findViewById(R.id.rbGearAnimation)).setChecked(true);
        } else if (savedAnimation.equals("ab_quilt")) {
            ((RadioButton) findViewById(R.id.rbQuiltAnimation)).setChecked(true);
        } else if (savedAnimation.equals("none")) {
            ((RadioButton) findViewById(R.id.rbNoAnimation)).setChecked(true);
        } else if (savedAnimation.equals("folder")) {
            ((RadioButton) findViewById(R.id.rbFolderAnimation)).setChecked(true);
            savedFolderUri = preferences.getString("png_folder_uri", null);
            if (savedFolderUri != null) {
                Uri folderUri = Uri.parse(savedFolderUri);
                loadFramesFromFolder(folderUri);
            }
        } else {
            ((RadioButton) findViewById(R.id.rbDefaultAnimation)).setChecked(true);
        }

// Re-enable and attach the listener after setting the state
        animationSelectorGroup.setOnCheckedChangeListener((group, checkedId) -> {
            SharedPreferences.Editor editor = preferences.edit();
            if (checkedId == R.id.rbCustomWallpaper) {
                selectWallpaperButton.setVisibility(View.VISIBLE);
                backgroundView.setVisibility(View.GONE);
                editor.putString("selected_animation", "custom_wallpaper");
            } else {
                selectWallpaperButton.setVisibility(View.GONE);
                backgroundView.setVisibility(View.VISIBLE);

                // Handle other animation options here
                if (checkedId == R.id.rbGearAnimation) {
                    backgroundView.setAnimation("ab_gear");
                    editor.putString("selected_animation", "ab_gear");
                } else if (checkedId == R.id.rbQuiltAnimation) {
                    backgroundView.setAnimation("ab_quilt");
                    editor.putString("selected_animation", "ab_quilt");
                } else if (checkedId == R.id.rbDefaultAnimation) {
                    backgroundView.setAnimation("ab");
                    editor.putString("selected_animation", "ab");
                } else if (checkedId == R.id.rbNoAnimation) {
                    backgroundView.stopAnimation();
                    backgroundView.setVisibility(View.GONE);
                    editor.putString("selected_animation", "none");
                } else if (checkedId == R.id.rbFolderAnimation) {
                    editor.putString("selected_animation", "folder");
                    selectPngFolder();
                }
            }
            editor.apply();
            backgroundView.startAnimation();
        });

// Handle the saved animation for the TiledBackgroundView
        if (savedAnimation.equals("ab_gear")) {
            backgroundView.setAnimation("ab_gear");
        } else if (savedAnimation.equals("ab_quilt")) {
            backgroundView.setAnimation("ab_quilt");
        } else if (savedAnimation.equals("none")) {
            backgroundView.stopAnimation();
            backgroundView.setVisibility(View.GONE); // Hide when animation stops
        } else if (savedAnimation.equals("folder")) {
            savedFolderUri = preferences.getString("png_folder_uri", null);
            if (savedFolderUri != null) {
                Uri folderUri = Uri.parse(savedFolderUri);
                loadFramesFromFolder(folderUri);
            }
        } else {
            backgroundView.setAnimation("ab");
        }
        backgroundView.startAnimation();

        // Launch file picker when button is clicked
        selectWallpaperButton.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.setType("image/*");
            startActivityForResult(intent, REQUEST_CODE_SELECT_WALLPAPER);
        });


        if ("custom_wallpaper".equals(savedAnimation)) {
            String savedWallpaperPath = preferences.getString(WALLPAPER_PREF_KEY, null);
            if (savedWallpaperPath != null) {
                File wallpaperFile = new File(savedWallpaperPath);
                if (wallpaperFile.exists()) {
                    try {
                        applyWallpaper(Uri.fromFile(wallpaperFile), preferences.getString(WALLPAPER_DISPLAY_PREF_KEY, "center"));
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                    }
                }
            }
        } else {
            // Apply other animation options as per the selected animation value
            applyAnimationBasedOnState(backgroundView, savedAnimation);
        }


        RadioGroup parallaxModeGroup = findViewById(R.id.parallaxModeGroup);

        String savedParallaxMode = preferences.getString("parallax_mode", "default");
// Pre-check whichever is saved
        switch (savedParallaxMode) {
            case "off":
                ((RadioButton) findViewById(R.id.rbParallaxOff)).setChecked(true);
                break;
            case "slow":
                ((RadioButton) findViewById(R.id.rbParallaxSlow)).setChecked(true);
                break;
            case "fast":
                ((RadioButton) findViewById(R.id.rbParallaxFast)).setChecked(true);
                break;
            default:
                ((RadioButton) findViewById(R.id.rbParallaxDefault)).setChecked(true);
                break;
        }

        applyParallaxMode(savedParallaxMode);

        parallaxModeGroup.setOnCheckedChangeListener((group, checkedId) -> {
            String mode;
            switch (checkedId) {
                case R.id.rbParallaxOff:
                    mode = "off";
                    break;
                case R.id.rbParallaxSlow:
                    mode = "slow";
                    break;
                case R.id.rbParallaxFast:
                    mode = "fast";
                    break;
                default:
                    mode = "default";
                    break;
            }
            preferences.edit().putString("parallax_mode", mode).apply();

            // Immediately apply to TiledBackgroundView:
            applyParallaxMode(mode);
        });



        // Find the YouTube URL input and Load Video button
        EditText youtubeUrlInput = findViewById(R.id.youtubeUrlInput);
        Button loadVideoButton = findViewById(R.id.loadVideoButton);

        // Set default video (this will be used if no custom URL is provided)
        final String defaultVideoId = "yNwKYgM6SkM"; // Wii shop channel music extended


        LinearLayout settingsLayout = findViewById(R.id.settingsLayout);


        // Override API key if custom key is set
        boolean isCustomApiKeyEnabled = preferences.getBoolean("enable_custom_api_key", false);
        if (isCustomApiKeyEnabled) {
            String customApiKey = preferences.getString("custom_api_key", "");
            if (customApiKey != null && !customApiKey.isEmpty()) {
                API_KEY = customApiKey;
            }
        }

        // Find the "Disable BG Music" button
        Button disableBgMusicButton = findViewById(R.id.disableBgMusicButton);

        // Load the saved BG music state from SharedPreferences
        boolean isBgMusicEnabled = preferences.getBoolean("bg_music_enabled", true);

        // Update the button text based on the current state
        updateBgMusicButtonText(disableBgMusicButton, isBgMusicEnabled);

        // Set the listener for the "Disable BG Music" button
        disableBgMusicButton.setOnClickListener(v -> {
            boolean currentBgMusicState = preferences.getBoolean("bg_music_enabled", true);
            boolean newBgMusicState = !currentBgMusicState;

            SharedPreferences.Editor editor = preferences.edit();
            editor.putBoolean("bg_music_enabled", newBgMusicState);
            editor.apply();

            updateBgMusicButtonText(disableBgMusicButton, newBgMusicState);

            // Enable or disable music based on the new state
            if (newBgMusicState) {
                onResume(); // Restart music based on the current selection (MP3 or YouTube)
            } else {
                stopBackgroundMusic(); // Stop both YouTube and MP3
            }
        });


        Button selectMp3Button = findViewById(R.id.selectMp3Button);
        selectMp3Button.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
            intent.setType("audio/mpeg");
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            startActivityForResult(intent, REQUEST_CODE_SELECT_MP3);
        });



        RadioGroup musicSourceGroup = findViewById(R.id.musicSourceGroup);
        RadioButton youtubeRadioButton = findViewById(R.id.youtubeRadioButton);
        RadioButton mp3RadioButton = findViewById(R.id.mp3RadioButton);

        // Initialize the Reset MP3 button
        Button resetMp3Button = findViewById(R.id.resetMp3Button);
        resetMp3Button.setOnClickListener(v -> {
            stopBackgroundMusic();

            // Reset the selected MP3 path in SharedPreferences
            SharedPreferences.Editor editor = preferences.edit();
            editor.remove("selected_mp3_path");
            editor.putString("music_source", "mp3"); // Ensure the music source is set to MP3
            editor.apply();

            // Update the RadioButton to reflect the change
            mp3RadioButton.setChecked(true);

            // Play the default MP3 from assets
            playDefaultMp3FromAssets();

            // Provide feedback to the user
            Toast.makeText(this, "MP3 reset to default", Toast.LENGTH_SHORT).show();
        });


        // Load saved preference
        String musicSource = preferences.getString("music_source", "mp3");

        if ("mp3".equals(musicSource)) {
            mp3RadioButton.setChecked(true);
        } else {
            youtubeRadioButton.setChecked(true);
        }

        // Save the selection to SharedPreferences
        musicSourceGroup.setOnCheckedChangeListener((group, checkedId) -> {
            SharedPreferences.Editor editor = preferences.edit();
            if (checkedId == R.id.youtubeRadioButton) {
                editor.putString("music_source", "youtube");
            } else if (checkedId == R.id.mp3RadioButton) {
                editor.putString("music_source", "mp3");
            }
            editor.apply();
        });


        webView = findViewById(R.id.webView);
        webView.getSettings().setJavaScriptEnabled(true); // Enable JavaScript
        webView.setWebViewClient(new WebViewClient()); // Prevent redirecting to external browser

        // Load saved preference for music source
        String selectedMp3Path = preferences.getString("selected_mp3_path", null);

        // Start music based on the selected source, only if BG music is enabled
        if (isBgMusicEnabled) {
            if ("mp3".equals(musicSource)) {
                // If selectedMp3Path is null, use the default mp3 from assets
                if (selectedMp3Path != null) {
                    // Get the MP3 file from internal storage
                    File mp3File = new File(selectedMp3Path);

                    // Play the selected MP3 if the user chose MP3
                    if (mp3File.exists()) {
                        playMp3(mp3File);  // Pass the File object to playMp3 method
                    } else {
                        Log.e("BigPictureActivity", "MP3 file not found: " + selectedMp3Path);
                        playDefaultMp3FromAssets();
                    }
                } else {
                    // No mp3 selected, play default mp3 from assets
                    playDefaultMp3FromAssets();
                }
            } else if ("youtube".equals(musicSource)) {
                // Play the YouTube video if the user chose YouTube
                String savedUrl = preferences.getString("saved_youtube_url", "");
                String videoId = savedUrl.isEmpty() ? defaultVideoId : extractYouTubeId(savedUrl);

                if (videoId != null) {
                    loadYouTubeVideo(videoId);
                    youtubeUrlInput.setText(savedUrl);  // Populate the input field with the saved URL
                } else {
                    // If no saved URL or invalid, use the default video
                    loadYouTubeVideo(defaultVideoId);
                    youtubeUrlInput.setText("");  // Clear the input field if invalid or default video is used
                }
            }
        }




        // Set the listener for the "Load Video" button
        loadVideoButton.setOnClickListener(v -> {
            String userUrl = youtubeUrlInput.getText().toString();
            if (userUrl != null && !userUrl.isEmpty()) {
                String videoId = extractYouTubeId(userUrl);
                if (videoId != null) {
                    loadYouTubeVideo(videoId);  // Load the user-specified video

                    // Save the YouTube URL to SharedPreferences
                    SharedPreferences.Editor editor = preferences.edit();
                    editor.putString("saved_youtube_url", userUrl);
                    editor.apply();
                } else {
                    // Show an error message if the URL is invalid
                    youtubeUrlInput.setError("Invalid YouTube URL");
                }
            } else {
                // Load the default video if no URL is entered
                loadYouTubeVideo(defaultVideoId);
            }
        });

        // Set immersive mode
        enableImmersiveMode();

        // Find the settings button
        ImageButton settingsButton = findViewById(R.id.settingsButton);

        // Tint the settings button icon to white
        Drawable settingsIcon = settingsButton.getDrawable();
        if (settingsIcon != null) {
            settingsIcon.mutate();  // Ensure it doesn't affect other instances
            settingsIcon.setColorFilter(Color.WHITE, PorterDuff.Mode.SRC_IN);  // Apply the white color filter
        }

        // Set the click listener for the settings button
        settingsButton.setOnClickListener(v -> {
            if (findViewById(R.id.settingsLayout).getVisibility() == View.VISIBLE) {
                hideSettingsView();
            } else {
                showSettingsView();
            }
        });

        // Find the back button
        ImageButton backButton = findViewById(R.id.backButton);

        // Tint the settings button icon to white
        Drawable backIcon = backButton.getDrawable();
        if (backIcon != null) {
            backIcon.mutate();  // Ensure it doesn't affect other instances
            backIcon.setColorFilter(Color.WHITE, PorterDuff.Mode.SRC_IN);  // Apply the white color filter
        }

        // Set the click listener for the settings button
        backButton.setOnClickListener(v -> {
            if (findViewById(R.id.settingsLayout).getVisibility() == View.VISIBLE) {
                hideSettingsView();
            } else {
                showSettingsView();
            }
        });


        coverArtView = findViewById(R.id.IVCoverArt);
        gameTitleView = findViewById(R.id.TVGameTitle);
        graphicsDriverView = findViewById(R.id.TVGraphicsDriver);
        graphicsDriverVersionView = findViewById(R.id.TVGraphicsDriverVersion);
        dxWrapperView = findViewById(R.id.TVDXWrapper);
        dxWrapperConfigView = findViewById(R.id.TVDXWrapperConfig);
        audioDriverView = findViewById(R.id.TVAudioDriver);
        box64PresetView = findViewById(R.id.TVBox64Preset);
        playCountView = findViewById(R.id.TVPlayCount);
        playtimeView = findViewById(R.id.TVPlaytime);
        recyclerView = findViewById(R.id.RecyclerView);
        playButton = findViewById(R.id.playButton);

        // Tint the play button icon to white
        Drawable playIcon = playButton.getDrawable();
        if (playIcon != null) {
            playIcon.mutate();  // Ensure it doesn't affect other instances
            playIcon.setColorFilter(Color.WHITE, PorterDuff.Mode.SRC_IN);  // Apply the white color filter
        }

        // Add item decoration for reduced spacing
        recyclerView.addItemDecoration(new CarouselItemDecoration(15));  // Reduced space between items

        // Initialize ContainerManager
        manager = new ContainerManager(this);

        // Load the list of shortcuts
        loadShortcutsList();

        // Setup snapping for RecyclerView to center the items
        SnapHelper snapHelper = new LinearSnapHelper();
        snapHelper.attachToRecyclerView(recyclerView);


        // Set the click listener for the play button
        playButton.setOnClickListener(v -> {
            if (currentShortcut != null) {
                runFromShortcut(currentShortcut);  // Use the loaded shortcut
            }
        });

        coverArtView.setOnClickListener(v -> {
            if (currentShortcut != null) {
                if (currentShortcut.getCustomCoverArtPath() != null) {
                    // Custom cover art exists, show the dialog
                    showCoverArtOptionsDialog();
                } else {
                    // No custom cover art, directly prompt for uploading a new one
                    promptForCustomCoverArtUpload();
                }
            }
        });

        playButton.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    playButton.requestFocus(); // Ensure playButton gets focus
                    playButton.performClick(); // Simulate a click immediately
                }
                return true; // Consumes the touch event so that it doesn't require another click
            }
        });

        settingsButton.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    settingsButton.requestFocus(); // Ensure settingsButton gets focus
                    settingsButton.performClick(); // Simulate a click immediately
                }
                return true; // Consumes the touch event
            }
        });

        backButton.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getAction() == MotionEvent.ACTION_DOWN) {
                    backButton.requestFocus(); // Ensure backButton gets focus
                    backButton.performClick(); // Simulate a click immediately
                }
                return true; // Consumes the touch event
            }
        });

    }

    @Override
    public void onBackPressed() {
        if (findViewById(R.id.settingsLayout).getVisibility() == View.VISIBLE) {
            hideSettingsView();
        } else {
            super.onBackPressed();
        }
    }

    private void updateBgMusicButtonText(Button button, boolean isEnabled) {
        if (isEnabled) {
            button.setText("Disable BG Music");
        } else {
            button.setText("Enable BG Music");
        }
    }


    /**
     * Extracts the YouTube video ID from a given URL.
     *
     * @param youtubeUrl The full YouTube URL.
     * @return The extracted video ID or null if the URL is invalid.
     */
    private String extractYouTubeId(String youtubeUrl) {
        String videoIdPattern = "^(https?://)?(www\\.)?(youtube\\.com|youtu\\.?be)/.+$";
        if (youtubeUrl.matches(videoIdPattern)) {
            String[] splitUrl = youtubeUrl.split("v=");
            if (splitUrl.length > 1) {
                // Video ID is after "v=" in standard URLs
                return splitUrl[1].split("&")[0]; // Split off any extra parameters
            } else if (youtubeUrl.contains("youtu.be/")) {
                // Handle short URLs (e.g., https://youtu.be/video_id)
                return youtubeUrl.substring(youtubeUrl.lastIndexOf("/") + 1);
            }
        }
        return null; // Invalid URL
    }

    private void loadYouTubeVideo(String videoId) {
        String html = "<html><body>" +
                "<iframe id=\"player\" type=\"text/html\" width=\"100%\" height=\"100%\"" +
                "src=\"https://www.youtube.com/embed/" + videoId + "?enablejsapi=1\"" +  // Removed autoplay and simplified script
                "frameborder=\"0\" allowfullscreen></iframe>" +
                "</body></html>";

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                // Trigger the native touch event on the WebView when the page is fully loaded
                simulateTouchOnWebView(webView);
                webView.setVisibility(View.INVISIBLE);
            }
        });

        webView.loadData(html, "text/html", "UTF-8");
    }

//    private void moveWebViewToBottom() {
//        // Get the parent of the WebView
//        FrameLayout parent = (FrameLayout) webView.getParent();
//
//        if (parent != null) {
//            // Remove the WebView from its parent
//            parent.removeView(webView);
//
//            // Add the WebView back to the parent at index 0 (bottom of the stack)
//            parent.addView(webView, 0);
//        }
//    }


    // Simulate a touch event at the center of the WebView
    private void simulateTouchOnWebView(WebView webView) {
        // Delay to allow WebView to fully load the content
        new Handler().postDelayed(() -> {
            // Get the WebView's dimensions
            int webViewWidth = webView.getWidth();
            int webViewHeight = webView.getHeight();

            // Calculate the center coordinates (YouTube player is usually in the center)
            float x = webViewWidth / 2f;
            float y = webViewHeight / 2f;

            // Create MotionEvent to simulate touch at the calculated position
            long downTime = System.currentTimeMillis();
            long eventTime = System.currentTimeMillis() + 100;

            // Simulate the touch press event
            MotionEvent touchDown = MotionEvent.obtain(
                    downTime, eventTime, MotionEvent.ACTION_DOWN, x, y, 0
            );

            // Simulate the touch release event
            MotionEvent touchUp = MotionEvent.obtain(
                    downTime, eventTime + 100, MotionEvent.ACTION_UP, x, y, 0
            );

            // Dispatch the events to the WebView
            webView.dispatchTouchEvent(touchDown);
            webView.dispatchTouchEvent(touchUp);

            // Recycle the events to free up memory
            touchDown.recycle();
            touchUp.recycle();


        }, 1000);  // Delay of 1 second after WebView loads
    }

    private void showSettingsView() {
        final LinearLayout mainLayout = findViewById(R.id.mainLayout);
        final LinearLayout settingsLayout = findViewById(R.id.settingsLayout);

        settingsLayout.setVisibility(View.VISIBLE);

        settingsLayout.getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                settingsLayout.getViewTreeObserver().removeOnPreDrawListener(this);
                // Start your animations here
                ObjectAnimator mainSlideOut = ObjectAnimator.ofFloat(mainLayout, "translationX", 0f, -mainLayout.getWidth());
                mainSlideOut.setInterpolator(new AccelerateDecelerateInterpolator());
                mainSlideOut.setDuration(500);
                mainSlideOut.start();

                ObjectAnimator settingsSlideIn = ObjectAnimator.ofFloat(settingsLayout, "translationX", settingsLayout.getWidth(), 0f);
                settingsSlideIn.setInterpolator(new AccelerateDecelerateInterpolator());
                settingsSlideIn.setDuration(500);
                settingsSlideIn.start();
                return true;
            }
        });
    }

    private void hideSettingsView() {
        LinearLayout mainLayout = findViewById(R.id.mainLayout);
        LinearLayout settingsLayout = findViewById(R.id.settingsLayout);

        // Slide the main layout back into view
        ObjectAnimator mainSlideIn = ObjectAnimator.ofFloat(mainLayout, "translationX", -mainLayout.getWidth(), 0f);
        mainSlideIn.setInterpolator(new AccelerateDecelerateInterpolator());
        mainSlideIn.setDuration(500);
        mainSlideIn.start();

        // Slide the settings layout off-screen to the right
        ObjectAnimator settingsSlideOut = ObjectAnimator.ofFloat(settingsLayout, "translationX", 0f, settingsLayout.getWidth());
        settingsSlideOut.setInterpolator(new AccelerateDecelerateInterpolator());
        settingsSlideOut.setDuration(500);
        settingsSlideOut.start();
        settingsSlideOut.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                settingsLayout.setVisibility(View.GONE);  // Hide after animation completes
            }
        });
    }


    private void showCoverArtOptionsDialog() {
        // Create an AlertDialog to show the options
        new androidx.appcompat.app.AlertDialog.Builder(this)
                .setTitle("Cover Art Options")
                .setItems(new CharSequence[]{"Remove Custom Cover Art", "Upload New Cover Art"}, (dialog, which) -> {
                    switch (which) {
                        case 0: // Remove Custom Cover Art
                            removeCustomCoverArt();
                            break;
                        case 1: // Upload New Cover Art
                            promptForCustomCoverArtUpload();
                            break;
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void removeCustomCoverArt() {
        if (currentShortcut != null) {
            Log.d("BigPictureActivity", "Removing cover art for shortcut: " + currentShortcut.name);
            Log.d("BigPictureActivity", "Current custom cover art path: " + currentShortcut.getCustomCoverArtPath());

            // Remove custom cover art from the shortcut
            currentShortcut.removeCustomCoverArt();

            // Clear cached cover art if exists
            File cachedFile = new File(getCacheDir(), "coverArtCache/" + currentShortcut.name + ".png");
            if (cachedFile.exists() && cachedFile.delete()) {
                Log.d("BigPictureActivity", "Cached cover art deleted successfully.");
            } else {
                Log.e("BigPictureActivity", "Failed to delete cached cover art or it doesn't exist.");
            }

            // Update UI
            coverArtView.setImageResource(R.drawable.icon_action_bar_import); // Default placeholder image
            coverArtView.setBackgroundColor(Color.parseColor("#99000000")); // Semi-transparent background

            Log.d("BigPictureActivity", "Custom cover art removed and data saved.");

            // Reload the shortcut data to reflect the changes
            loadShortcutData(currentShortcut);
            Log.d("BigPictureActivity", "Shortcut data reloaded after removal.");
        }
    }



    // Prompt user to select a custom cover art image from gallery
    private void promptForCustomCoverArtUpload() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("image/*");
//        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, false);
        startActivityForResult(intent, REQUEST_CODE_UPLOAD_CUSTOM_COVER);
    }

    private void enableImmersiveMode() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    private Shortcut getSelectedShortcut() {
        int position = getCenterItemPosition();
        if (position != RecyclerView.NO_POSITION) {
            return adapter.getItem(position);
        }
        return null;
    }

    private int getCenterItemPosition() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        int firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition();
        int lastVisibleItemPosition = layoutManager.findLastVisibleItemPosition();

        int centerPosition = RecyclerView.NO_POSITION;
        float closestToCenter = Float.MAX_VALUE;
        int recyclerViewCenter = recyclerView.getWidth() / 2;

        for (int i = firstVisibleItemPosition; i <= lastVisibleItemPosition; i++) {
            if (i >= 0) {
                View itemView = layoutManager.findViewByPosition(i);
                if (itemView != null) {
                    int itemCenter = (itemView.getLeft() + itemView.getRight()) / 2;
                    float distanceFromCenter = Math.abs(recyclerViewCenter - itemCenter);

                    if (distanceFromCenter < closestToCenter) {
                        closestToCenter = distanceFromCenter;
                        centerPosition = i;
                    }
                }
            }
        }

        return centerPosition;
    }

    private void loadShortcutsList() {
        List<Shortcut> shortcuts = manager.loadShortcuts();
        emptyStateTextView = findViewById(R.id.TVEmptyState);

        // Check if the shortcuts list is empty
        if (shortcuts.isEmpty()) {
            // Show the empty state message
            recyclerView.setVisibility(View.GONE);
            playButton.setVisibility(View.GONE);
            emptyStateTextView.setVisibility(View.VISIBLE);
        } else {
            // Hide the empty state message and show the RecyclerView
            recyclerView.setVisibility(View.VISIBLE);
            emptyStateTextView.setVisibility(View.GONE);

            // Initialize and set the adapter
            adapter = new BigPictureAdapter(shortcuts, recyclerView); // Pass the RecyclerView reference
            recyclerView.setLayoutManager(new LinearLayoutManager(this, LinearLayoutManager.HORIZONTAL, false));
            recyclerView.setAdapter(adapter);

            // Preload first shortcut details
            loadShortcutData(shortcuts.get(0));
        }
    }


    public void loadShortcutData(Shortcut shortcut) {
        currentShortcut = shortcut;

        // Log the current cover art path
        Log.d("BigPictureActivity", "Loaded cover art path: " + shortcut.getCustomCoverArtPath());

        // Set the game title
        gameTitleView.setText(shortcut.name);

        // Set play count and playtime (unchanged)
        SharedPreferences playtimePrefs = getSharedPreferences("playtime_stats", Context.MODE_PRIVATE);
        long totalPlaytime = playtimePrefs.getLong(shortcut.name + "_playtime", 0);
        int playCount = playtimePrefs.getInt(shortcut.name + "_play_count", 0);
        playCountView.setText("Times Played: " + playCount);
        playtimeView.setText("Playtime: " + formatPlaytime(totalPlaytime));

        // Get the associated container for this shortcut (unchanged)
        Container container = manager.getContainerForShortcut(shortcut);
        String graphicsDriver = shortcut.getExtra("graphicsDriver");
        
        setTextOrPlaceholder(graphicsDriverView, graphicsDriver, container.getGraphicsDriver());
        setTextOrPlaceholder(graphicsDriverVersionView, shortcut.getExtra("graphicsDroverConfig"), container.getGraphicsDriverConfig());
        setTextOrPlaceholder(dxWrapperView, shortcut.getExtra("dxwrapper"), container.getDXWrapper());
        setTextOrPlaceholder(dxWrapperConfigView, shortcut.getExtra("dxwrapperConfig"), container.getDXWrapperConfig());
        setTextOrPlaceholder(audioDriverView, shortcut.getExtra("audioDriver"), container.getAudioDriver());
        setTextOrPlaceholder(box64PresetView, shortcut.getExtra("box64Preset"), container.getBox64Preset());

        // Handle cover art loading
        Bitmap coverArt = null;
        if (shortcut.getCustomCoverArtPath() != null && !shortcut.getCustomCoverArtPath().isEmpty()) {
            coverArt = BitmapFactory.decodeFile(shortcut.getCustomCoverArtPath());
        }

        if (coverArt == null) {
            // Check for cached cover art if custom cover art is not found or removed
            coverArt = loadCachedCoverArt(shortcut.name);
        }

        if (coverArt != null) {
            coverArtView.setImageBitmap(coverArt); // Set cover art from custom or cache
        } else {
            coverArtView.setImageResource(R.drawable.cover_art_placeholder); // Default icon or placeholder
            fetchCoverArt(shortcut); // Fetch from remote if not available locally
        }

        // Update the click listener to reflect the current state of custom cover art
        coverArtView.setOnClickListener(v -> {
            if (currentShortcut.getCustomCoverArtPath() != null) {
                // Custom cover art exists, show the dialog
                showCoverArtOptionsDialog();
            } else {
                // No custom cover art, directly prompt for uploading a new one
                promptForCustomCoverArtUpload();
            }
        });
    }


    private void runFromShortcut(Shortcut shortcut) {
        // Launch XServerDisplayActivity with the necessary extras
        Intent intent = new Intent(this, XServerDisplayActivity.class);
        intent.putExtra("container_id", shortcut.container.id);
        intent.putExtra("shortcut_path", shortcut.file.getPath());
        intent.putExtra("shortcut_name", shortcut.name); // Pass the shortcut name for display
        // Check if the shortcut has the disableXinput value; if not, default to false.
        String disableXinputValue = shortcut.getExtra("disableXinput", "0"); // Get value from shortcut or use "0" (false) by default
        intent.putExtra("disableXinput", disableXinputValue); // Use the actual value from the shortcut
        startActivity(intent);
    }


    private void setTextOrPlaceholder(TextView textView, String shortcutValue, String containerValue) {
        if (!shortcutValue.isEmpty()) {
            textView.setText(shortcutValue); // Use the value from the shortcut if available
        } else if (!containerValue.isEmpty()) {
            textView.setText(containerValue); // Fallback to the container's value
        } else {
            textView.setText("Not Set"); // Fallback if neither are available
        }
    }


    private void setTextFromContainer(TextView textView, String label, String shortcutValue, String containerValue) {
        if (!shortcutValue.isEmpty()) {
            textView.setText(label + shortcutValue); // Use the value from the shortcut if available
        } else if (!containerValue.isEmpty()) {
            textView.setText(label + containerValue); // Fallback to the container's value
        } else {
            textView.setText(label + "Not Set"); // Fallback if neither are available
        }
    }


    private void fetchCoverArt(Shortcut shortcut) {
        Retrofit retrofit = new Retrofit.Builder()
                .baseUrl(BASE_URL)
                .client(new OkHttpClient())
                .addConverterFactory(GsonConverterFactory.create())
                .build();

        SteamGridDBApi api = retrofit.create(SteamGridDBApi.class);
        Call<SteamGridSearchResponse> call = api.searchGame("Bearer " + API_KEY, shortcut.name);

        call.enqueue(new Callback<SteamGridSearchResponse>() {
            @Override
            public void onResponse(Call<SteamGridSearchResponse> call, Response<SteamGridSearchResponse> response) {
                if (response.isSuccessful() && response.body() != null) {
                    List<SteamGridSearchResponse.GameData> gameData = response.body().data;
                    if (gameData != null && !gameData.isEmpty()) {
                        fetchGridsForGame(gameData.get(0).id, shortcut);
                    } else {
                        showCustomCoverArtUploadOption(shortcut);
                    }
                } else {
                    showCustomCoverArtUploadOption(shortcut);
                }
            }

            @Override
            public void onFailure(Call<SteamGridSearchResponse> call, Throwable t) {
                Log.e("SteamGridDB", "Failed to fetch game ID", t);
                showCustomCoverArtUploadOption(shortcut);
            }
        });
    }

    // Display upload custom cover art option if no cover art found
    private void showCustomCoverArtUploadOption(Shortcut shortcut) {
        runOnUiThread(() -> {
            coverArtView.setImageResource(android.R.color.transparent); // Remove existing placeholder
            coverArtView.setBackgroundColor(Color.parseColor("#99000000")); // Semi-transparent gray background

            // Set the image resource to show upload icon and update the click listener
            coverArtView.setImageResource(R.drawable.cover_art_placeholder);
            coverArtView.setOnClickListener(v -> promptForCustomCoverArtUpload());

            // Display message indicating no cover art found
            if (uploadText != null) {
                ViewGroup parent = (ViewGroup) uploadText.getParent();
                if (parent != null) {
                    parent.removeView(uploadText);
                }
            }

            uploadText = new TextView(this); // Initialize the uploadText variable
            uploadText.setText("No suitable cover art found for " + shortcut.name + ". Click the image to upload custom cover art or rename the Shortcut to something SteamGrid can recognize.");
            uploadText.setTextColor(Color.WHITE);
            uploadText.setTextSize(18);
            uploadText.setPadding(20, 20, 20, 20);
            uploadText.setGravity(Gravity.CENTER);
            uploadText.setBackgroundColor(Color.parseColor("#99000000")); // Semi-transparent gray background

            // Add the text view as a sibling to cover art view
            ViewGroup parent = (ViewGroup) coverArtView.getParent();
            parent.addView(uploadText);
        });
    }




    private void fetchGridsForGame(int gameId, Shortcut shortcut) {
        Gson gson = new GsonBuilder()
                .registerTypeAdapter(SteamGridGridsResponse.class, new SteamGridGridsResponseDeserializer())
                .setPrettyPrinting()
                .create();

        Retrofit retrofit = new Retrofit.Builder()
                .baseUrl(BASE_URL)
                .client(new OkHttpClient())
                .addConverterFactory(GsonConverterFactory.create(gson))
                .build();

        SteamGridDBApi api = retrofit.create(SteamGridDBApi.class);

        Call<SteamGridGridsResponse> gridsCall = api.getGridsByGameId("Bearer " + API_KEY, gameId, "alternate", "600x900", "static");

        gridsCall.enqueue(new Callback<SteamGridGridsResponse>() {
            @Override
            public void onResponse(Call<SteamGridGridsResponse> call, Response<SteamGridGridsResponse> response) {
                if (response.isSuccessful() && response.body() != null && !response.body().data.isEmpty()) {
                    downloadCoverArt(response.body().data.get(0).url, shortcut);
                }
            }

            @Override
            public void onFailure(Call<SteamGridGridsResponse> call, Throwable t) {
                Log.e("SteamGridDB", "Failed to fetch cover art", t);
            }
        });
    }

    private void downloadCoverArt(String url, Shortcut shortcut) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
                connection.connect();
                InputStream input = connection.getInputStream();
                Bitmap coverArt = BitmapFactory.decodeStream(input);

                // Cache the downloaded cover art
                cacheCoverArt(coverArt, shortcut.name);

                // Set cover art in the shortcut and update the UI
                shortcut.setCoverArt(coverArt);
                runOnUiThread(() -> coverArtView.setImageBitmap(coverArt));
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
    }

    private void cacheCoverArt(Bitmap coverArt, String shortcutName) {
        try {
            File cacheDir = new File(getCacheDir(), "coverArtCache");
            if (!cacheDir.exists()) {
                cacheDir.mkdirs();
            }
            File coverFile = new File(cacheDir, shortcutName + ".png");
            FileOutputStream outputStream = new FileOutputStream(coverFile);
            coverArt.compress(Bitmap.CompressFormat.PNG, 100, outputStream);
            outputStream.flush();
            outputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private Bitmap loadCachedCoverArt(String shortcutName) {
        try {
            File cacheDir = new File(getCacheDir(), "coverArtCache");
            File coverFile = new File(cacheDir, shortcutName + ".png");
            if (coverFile.exists()) {
                return BitmapFactory.decodeFile(coverFile.getAbsolutePath());
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == REQUEST_CODE_SELECT_WALLPAPER && resultCode == RESULT_OK && data != null) {
            Uri selectedImageUri = data.getData();

            try {
                // Save the selected wallpaper as custom_bg.png
                InputStream inputStream = getContentResolver().openInputStream(selectedImageUri);
                Bitmap wallpaper = BitmapFactory.decodeStream(inputStream);
                if (wallpaper != null) {
                    File wallpaperFile = new File(getFilesDir(), "custom_bg.png");
                    FileOutputStream outputStream = new FileOutputStream(wallpaperFile);
                    wallpaper.compress(Bitmap.CompressFormat.PNG, 100, outputStream);
                    outputStream.flush();
                    outputStream.close();

                    // Save the path to SharedPreferences
                    SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
                    SharedPreferences.Editor editor = preferences.edit();
                    editor.putString(WALLPAPER_PREF_KEY, wallpaperFile.getAbsolutePath());
                    editor.apply();

                    // Show dialog for display preference (center, stretch, tile)
                    String[] displayOptions = {"Center", "Stretch", "Tile"};
                    new AlertDialog.Builder(this)
                            .setTitle("Select Display Mode")
                            .setItems(displayOptions, (dialog, which) -> {
                                // Save display mode
                                editor.putString(WALLPAPER_DISPLAY_PREF_KEY, displayOptions[which].toLowerCase());
                                editor.apply();

                                // Apply wallpaper based on the chosen mode
                                try {
                                    applyWallpaper(Uri.fromFile(wallpaperFile), displayOptions[which].toLowerCase());
                                } catch (FileNotFoundException e) {
                                    throw new RuntimeException(e);
                                }
                            })
                            .show();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }

        }

        if (requestCode == REQUEST_CODE_SELECT_MP3 && resultCode == RESULT_OK && data != null && data.getData() != null) {
            selectedMp3Uri = data.getData();

            // Get the internal storage directory
            File appStorageDir = getFilesDir();
            File musicFile = new File(appStorageDir, "bigpicturemode_bgmusic.mp3");

            // Use FileUtils to copy the selected MP3 to internal storage and rename it
            if (FileUtils.copy(this, selectedMp3Uri, musicFile, null)) {
                // Save the file path to SharedPreferences
                SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
                SharedPreferences.Editor editor = preferences.edit();
                editor.putString("selected_mp3_path", musicFile.getAbsolutePath()); // Store the file path instead of URI
                editor.apply();

                // Play the copied MP3 file using the updated playMp3 method
                playMp3(musicFile);  // Pass the File object
            } else {
                Log.e("BigPictureActivity", "Failed to copy the MP3 file.");
            }
        } else if (requestCode == REQUEST_CODE_UPLOAD_CUSTOM_COVER && data.getData() != null) {
            Uri selectedImageUri = data.getData();
            try {
                InputStream inputStream = getContentResolver().openInputStream(selectedImageUri);
                Bitmap customCoverArt = BitmapFactory.decodeStream(inputStream);

                // Save custom cover art in shortcut and cache it
                if (currentShortcut != null) {
                    currentShortcut.saveCustomCoverArt(customCoverArt);  // Save custom cover art

                    // Cache the custom cover art
                    cacheCoverArt(customCoverArt, currentShortcut.name);

                    // Set the custom cover art in the view
                    coverArtView.setImageBitmap(customCoverArt);

                    // Hide the upload text once the cover art is uploaded
                    if (uploadText != null) {
                        uploadText.setVisibility(View.GONE);
                    }

                    // Update the click listener to reflect that custom cover art is now present
                    coverArtView.setOnClickListener(v -> {
                        if (currentShortcut.getCustomCoverArtPath() != null) {
                            // Custom cover art exists, show the dialog
                            showCoverArtOptionsDialog();
                        } else {
                            // No custom cover art, directly prompt for uploading a new one
                            promptForCustomCoverArtUpload();
                        }
                    });
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        if (requestCode == REQUEST_CODE_SELECT_PNG_FOLDER && resultCode == RESULT_OK) {
            if (data != null) {
                Uri folderUri = data.getData();
                // Take persistable permissions:
                final int takeFlags = data.getFlags()
                        & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                        | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

                getContentResolver().takePersistableUriPermission(folderUri, takeFlags);

                // 1) Save to SharedPreferences
                SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
                prefs.edit()
                        .putString("png_folder_uri", folderUri.toString())
                        .putString("selected_animation", "folder")   // <-- store that we’re now using a folder-based animation
                        .apply();

                // 2) Load the frames
                loadFramesFromFolder(folderUri);
            }
        }

    }


    private void applyWallpaper(Uri wallpaperUri, String mode) throws FileNotFoundException {
        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);
        if (backgroundView != null && wallpaperUri != null) {
            Bitmap wallpaper = BitmapFactory.decodeStream(getContentResolver().openInputStream(wallpaperUri));
            if (wallpaper != null) {
                backgroundView.setVisibility(View.VISIBLE);
                backgroundView.setStaticWallpaper(wallpaper, mode);
            } else {
                Log.e("BigPictureActivity", "Invalid wallpaper dimensions.");
            }
        }
    }

    private void applyAnimationBasedOnState(TiledBackgroundView backgroundView, String animationState) {
        if (backgroundView != null && animationState != null) {
            switch (animationState) {
                case "ab_gear":
                    backgroundView.setAnimation("ab_gear");
                    break;
                case "ab_quilt":
                    backgroundView.setAnimation("ab_quilt");
                    break;
                case "none":
                    backgroundView.stopAnimation();
                    backgroundView.setVisibility(View.GONE); // Hide the view when animation is stopped
                    return; // Exit early since there's no animation to start
                case "folder":
                    // Do NOT call setAnimation(...) with "ab" or "ab_gear"
                    // Instead, do nothing special here, because we’ll load PNG frames from the folder in onResume or onCreate
                    // Possibly just stopAnimation if you want, or call start if frames are loaded
                    break;
                default:
                    if (!"folder".equals(animationState)) {
                        backgroundView.setAnimation("ab");
                    }
                    break;
            }
            backgroundView.startAnimation();
        }
    }




    private void playMp3(File mp3File) {
        if (mediaPlayer != null) {
            mediaPlayer.release();  // Release any existing player
        }

        mediaPlayer = new MediaPlayer();
        try {
            FileInputStream fis = new FileInputStream(mp3File);
            mediaPlayer.setDataSource(fis.getFD());  // Use FileDescriptor for internal storage files
            fis.close();

            mediaPlayer.prepare();  // Prepare synchronously
            mediaPlayer.setOnPreparedListener(mp -> mediaPlayer.start());  // Start playback once prepared
        } catch (IOException e) {
            e.printStackTrace();
        }
    }



//    private String saveCustomCoverArt(Bitmap coverArt, String shortcutName) {
//        try {
//            File coverArtDir = new File(currentShortcut.container.getRootDir(), "app_data/cover_arts"); // Match path with Shortcut class
//            if (!coverArtDir.exists()) {
//                coverArtDir.mkdirs(); // Create directory if not exist
//            }
//
//            File coverFile = new File(coverArtDir, shortcutName + ".png");
//            FileOutputStream outputStream = new FileOutputStream(coverFile);
//            coverArt.compress(Bitmap.CompressFormat.PNG, 100, outputStream);
//            outputStream.flush();
//            outputStream.close();
//            return coverFile.getAbsolutePath(); // Return the file path for storing in shortcut
//        } catch (IOException e) {
//            e.printStackTrace();
//        }
//        return null;
//    }


    private String formatPlaytime(long playtimeInMillis) {
        long seconds = (playtimeInMillis / 1000) % 60;
        long minutes = (playtimeInMillis / (1000 * 60)) % 60;
        long hours = (playtimeInMillis / (1000 * 60 * 60)) % 24;
        long days = (playtimeInMillis / (1000 * 60 * 60 * 24));

        return String.format("%dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            View currentFocus = getCurrentFocus();

            switch (event.getKeyCode()) {
                case KeyEvent.KEYCODE_DPAD_UP:
                    if (currentFocus == recyclerView) {
                        playButton.requestFocus(); // Move focus to playButton when UP is pressed from RecyclerView
                        return true;
                    } else if (currentFocus == playButton) {
                        // Move focus to the first TextView (e.g., graphicsDriverView) when UP is pressed from playButton
                        graphicsDriverView.requestFocus();
                        return true;
                    } else if (currentFocus != coverArtView) {
                        // Default behavior: Move focus to playButton from anywhere else (except coverArt)
                        playButton.requestFocus();
                        return true;
                    }
                    break;

                case KeyEvent.KEYCODE_DPAD_DOWN:
                    if (currentFocus == playButton) {
                        // Focus the closest carousel item to the center or playButton
                        focusClosestCarouselItem();
                        return true;
                    }
                    break;

                case KeyEvent.KEYCODE_DPAD_LEFT:
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                    break;

                case KeyEvent.KEYCODE_BUTTON_A:
                    if (currentFocus == playButton) {
                        // Simulate a click on the play button if A is pressed
                        playButton.performClick();
                        return true;
                    } else if (currentFocus == coverArtView) {
                        // Simulate a click on the cover art if A is pressed
                        coverArtView.performClick();
                        return true;
                    }
                    break;
                case KeyEvent.KEYCODE_BUTTON_R1:  // RB button
                case KeyEvent.KEYCODE_BUTTON_R2:  // RT button
                    if (findViewById(R.id.settingsLayout).getVisibility() == View.VISIBLE) {
                        hideSettingsView();
                    } else {
                        showSettingsView();
                    }
                    return true;

            }
        }

        return super.dispatchKeyEvent(event);
    }

    private void focusClosestCarouselItem() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        int firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition();
        int lastVisibleItemPosition = layoutManager.findLastVisibleItemPosition();

        int closestPosition = RecyclerView.NO_POSITION;
        float closestDistance = Float.MAX_VALUE;

        // Get the center of the recyclerView or use a constant for where the playButton is (center of screen)
        int recyclerViewCenter = recyclerView.getWidth() / 2;

        // Iterate over visible items to find the closest one to the center
        for (int i = firstVisibleItemPosition; i <= lastVisibleItemPosition; i++) {
            if (i >= 0) {
                View itemView = layoutManager.findViewByPosition(i);
                if (itemView != null) {
                    int itemCenter = (itemView.getLeft() + itemView.getRight()) / 2;
                    float distanceFromCenter = Math.abs(recyclerViewCenter - itemCenter);

                    if (distanceFromCenter < closestDistance) {
                        closestDistance = distanceFromCenter;
                        closestPosition = i;
                    }
                }
            }
        }

        // Focus the closest item in the carousel
        if (closestPosition != RecyclerView.NO_POSITION) {
            recyclerView.scrollToPosition(closestPosition);
            layoutManager.findViewByPosition(closestPosition).requestFocus();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(this);
        boolean isBgMusicEnabled = preferences.getBoolean("bg_music_enabled", true);
        String musicSource = preferences.getString("music_source", "mp3"); // Default to "mp3"
        String selectedMp3Path = preferences.getString("selected_mp3_path", null); // Use the file path instead of URI

        // Restore the selected animation state for the RadioGroup
        String savedAnimation = preferences.getString("selected_animation", "ab");
        if (savedAnimation.equals("custom_wallpaper")) {
            ((RadioButton) findViewById(R.id.rbCustomWallpaper)).setChecked(true);
        } else if (savedAnimation.equals("ab_gear")) {
            ((RadioButton) findViewById(R.id.rbGearAnimation)).setChecked(true);
        } else if (savedAnimation.equals("ab_quilt")) {
            ((RadioButton) findViewById(R.id.rbQuiltAnimation)).setChecked(true);
        } else if (savedAnimation.equals("none")) {
            ((RadioButton) findViewById(R.id.rbNoAnimation)).setChecked(true);
        } else if (savedAnimation.equals("folder")) {
            ((RadioButton) findViewById(R.id.rbFolderAnimation)).setChecked(true);
        } else {
            ((RadioButton) findViewById(R.id.rbDefaultAnimation)).setChecked(true);
        }

        if (!isBgMusicEnabled) {
            webView.setVisibility(View.GONE);
            return; // Exit early if background music is disabled
        }

        // Handle WebView visibility based on music source
        if ("mp3".equals(musicSource)) {
            if (webView != null) {
                webView.setVisibility(View.INVISIBLE); // Hide WebView when MP3 is selected
            }

            // Play MP3 if file exists
            if (selectedMp3Path != null) {
                File mp3File = new File(selectedMp3Path);
                if (mp3File.exists() && (mediaPlayer == null || !mediaPlayer.isPlaying())) {
                    playMp3(mp3File);  // Play the MP3 file
                }
            }
        } else if ("youtube".equals(musicSource)) {
            if (webView != null) {
                webView.setVisibility(View.VISIBLE); // Show WebView if YouTube is selected
            }

            String savedUrl = preferences.getString("saved_youtube_url", "");
            String videoId = savedUrl.isEmpty() ? "yNwKYgM6SkM" : extractYouTubeId(savedUrl);
            if (videoId != null) {
                loadYouTubeVideo(videoId);
            }
        }
    }

    private void playDefaultMp3FromAssets() {
        if (mediaPlayer != null) {
            mediaPlayer.release();  // Release any existing player
        }

        mediaPlayer = new MediaPlayer();
        try {
            AssetFileDescriptor afd = getAssets().openFd("default_music.mp3"); // Ensure the MP3 file is named "default_music.mp3" in assets
            mediaPlayer.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
            mediaPlayer.setLooping(true); // Loop the music
            mediaPlayer.prepare();
            mediaPlayer.start();
            afd.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }





    private void stopBackgroundMusic() {
        // Stop YouTube WebView
        if (webView != null) {
            webView.loadUrl("about:blank");
        }

        // Stop MP3 playback
        if (mediaPlayer != null && mediaPlayer.isPlaying()) {
            mediaPlayer.stop();
            mediaPlayer.release(); // Release resources
            mediaPlayer = null;
        }
    }


    @Override
    protected void onPause() {
        super.onPause();
        stopBackgroundMusic(); // Ensure MP3 or YouTube stops when activity is paused
    }

    // Import Animations:
    private static final int REQUEST_CODE_SELECT_PNG_FOLDER = 1090;

    private void selectPngFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        // Optionally, set some flags or extras if needed
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_CODE_SELECT_PNG_FOLDER);
    }

    private void loadFramesFromFolder(Uri folderUri) {
        // 1. DocumentFile from the tree URI
        DocumentFile docFolder = DocumentFile.fromTreeUri(this, folderUri);
        if (docFolder == null || !docFolder.isDirectory()) {
            Toast.makeText(this, "Invalid folder selected!", Toast.LENGTH_SHORT).show();
            return;
        }

        // 2. Iterate children
        DocumentFile[] docFiles = docFolder.listFiles();
        if (docFiles == null || docFiles.length == 0) {
            Toast.makeText(this, "No files in folder!", Toast.LENGTH_SHORT).show();
            return;
        }

        List<Bitmap> bitmaps = new ArrayList<>();
        for (DocumentFile df : docFiles) {
            if (df != null && df.getName() != null && df.getName().toLowerCase().endsWith(".png")) {
                try (InputStream is = getContentResolver().openInputStream(df.getUri())) {
                    Bitmap bmp = BitmapFactory.decodeStream(is);
                    if (bmp != null) {
                        bitmaps.add(bmp);
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }

        if (bitmaps.isEmpty()) {
            Toast.makeText(this, "No PNG files found in this folder!", Toast.LENGTH_SHORT).show();
            return;
        }

        Log.d("AnimationCheck", "Loaded " + bitmaps.size() + " PNG frames from folder.");
        // 3. Pass these bitmaps to the TiledBackgroundView
        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);
        backgroundView.loadFramesFromBitmaps(bitmaps);  // A new method we’ll create below
    }

    private void applyParallaxMode(String mode) {
        TiledBackgroundView backgroundView = findViewById(R.id.parallaxBackgroundView);
        if (backgroundView == null) return;

        switch (mode) {
            case "off":
                backgroundView.setParallax(false, 0f, 0f);
                break;
            case "slow":
                backgroundView.setParallax(true, 1.0f, 1.0f);
                break;
            case "fast":
                backgroundView.setParallax(true, 5.0f, 5.0f);
                break;
            default: // "default"
                backgroundView.setParallax(true, 2.0f, 2.0f);
                break;
        }
    }



}
