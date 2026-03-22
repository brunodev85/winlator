package com.winlator.contentdialog;

import android.app.Activity;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.google.android.material.progressindicator.CircularProgressIndicator;
import com.winlator.R;
import com.winlator.container.Container;
import com.winlator.core.Callback;
import com.winlator.core.FileUtils;
import com.winlator.core.StringUtils;

import java.io.File;
import java.util.concurrent.atomic.AtomicLong;

public class StorageInfoDialog extends ContentDialog {
    public StorageInfoDialog(@NonNull Activity activity, Container container) {
        super(activity, R.layout.container_storage_info_dialog);

        setTitle(R.string.storage_info);
        setIcon(R.drawable.icon_info);

        AtomicLong driveCSize = new AtomicLong();
        driveCSize.set(0);

        AtomicLong cacheSize = new AtomicLong();
        cacheSize.set(0);

        AtomicLong totalSize = new AtomicLong();
        totalSize.set(0);

        final TextView tvDriveCSize = findViewById(R.id.TVDriveCSize);
        final TextView tvCacheSize = findViewById(R.id.TVCacheSize);
        final TextView tvTotalSize = findViewById(R.id.TVTotalSize);
        final TextView tvUsedSpace = findViewById(R.id.TVUsedSpace);
        final CircularProgressIndicator circularProgressIndicator = findViewById(R.id.CircularProgressIndicator);

        final long internalStorageSize = FileUtils.getInternalStorageSize();

        Runnable updateUI = () -> {
            tvDriveCSize.setText(StringUtils.formatBytes(driveCSize.get()));
            tvCacheSize.setText(StringUtils.formatBytes(cacheSize.get()));
            tvTotalSize.setText(StringUtils.formatBytes(totalSize.get()));

            int progress = (int)(((double)totalSize.get() / internalStorageSize) * 100);
            tvUsedSpace.setText(progress+"%");
            circularProgressIndicator.setProgress(progress, true);
        };

        File rootDir = container.getRootDir();
        final File driveCDir = new File(rootDir, ".wine/drive_c");
        final File cacheDir = new File(rootDir, ".cache");
        AtomicLong lastTime = new AtomicLong(System.currentTimeMillis());

        final Callback<Long> onAddSize = (size) -> {
            totalSize.addAndGet(size);
            long currTime = System.currentTimeMillis();
            int elapsedTime = (int)(currTime - lastTime.get());
            if (elapsedTime > 30) {
                activity.runOnUiThread(updateUI);
                lastTime.set(currTime);
            }
        };

        FileUtils.getSizeAsync(driveCDir, (size) -> {
            driveCSize.addAndGet(size);
            onAddSize.call(size);
        });

        FileUtils.getSizeAsync(cacheDir, (size) -> {
            cacheSize.addAndGet(size);
            onAddSize.call(size);
        });

        ((TextView)findViewById(R.id.BTCancel)).setText(R.string.clear_cache);
        setOnCancelCallback(() -> {
            FileUtils.clear(cacheDir);

            container.putExtra("desktopTheme", null);
            container.saveData();
        });
    }
}
