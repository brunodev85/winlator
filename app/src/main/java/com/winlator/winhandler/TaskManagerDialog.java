package com.winlator.winhandler;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;

import com.winlator.R;
import com.winlator.XServerDisplayActivity;
import com.winlator.core.ProcessHelper;
import com.winlator.widget.CPUListView;
import com.winlator.contentdialog.ContentDialog;

import java.util.Timer;
import java.util.TimerTask;

public class TaskManagerDialog extends ContentDialog {
    private final XServerDisplayActivity activity;
    private final LayoutInflater inflater;
    private Timer timer;

    public TaskManagerDialog(XServerDisplayActivity activity) {
        super(activity, R.layout.task_manager_dialog);
        this.activity = activity;
        setCancelable(false);
        setTitle(R.string.task_manager);
        setIcon(R.drawable.icon_task_manager);

        Button cancelButton = findViewById(R.id.BTCancel);
        cancelButton.setText(R.string.new_task);
        cancelButton.setOnClickListener((v) -> {
            dismiss();
            ContentDialog.prompt(activity, R.string.new_task, "taskmgr.exe", (command) -> activity.getWinHandler().exec(command));
        });

        setOnDismissListener((dialog) -> {
            if (timer != null) {
                timer.cancel();
                timer = null;
            }
        });

        inflater = LayoutInflater.from(activity);
    }

    private void update() {
        WinHandler winHandler = activity.getWinHandler();
        winHandler.getProcesses((processes) -> activity.runOnUiThread(() -> {
            final LinearLayout container = findViewById(R.id.LLProcessList);
            container.removeAllViews();
            setBottomBarText(activity.getString(R.string.processes)+": "+processes.size());

            if (!processes.isEmpty()) {
                findViewById(R.id.TVEmptyText).setVisibility(View.GONE);

                for (final ProcessInfo processInfo : processes) {
                    View itemView = inflater.inflate(R.layout.process_info_list_item, container, false);
                    ((TextView)itemView.findViewById(R.id.TVName)).setText(processInfo.name);
                    ((TextView)itemView.findViewById(R.id.TVPID)).setText(String.valueOf(processInfo.pid));
                    ((TextView)itemView.findViewById(R.id.TVMemoryUsage)).setText(processInfo.getFormattedMemoryUsage());
                    itemView.findViewById(R.id.BTMenu).setOnClickListener((v) -> showListItemMenu(v, processInfo));
                    container.addView(itemView);
                }
            }
            else findViewById(R.id.TVEmptyText).setVisibility(View.VISIBLE);
        }));

        LinearLayout tableHead = findViewById(R.id.LLTableHead);
        TextView tvMemoryInfo = (TextView)tableHead.getChildAt(2);
        tvMemoryInfo.setText(activity.getString(R.string.memory)+" ("+ getMemoryUsagePercent()+"%)");
    }

    private void showListItemMenu(final View anchorView, final ProcessInfo processInfo) {
        PopupMenu listItemMenu = new PopupMenu(activity, anchorView);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) listItemMenu.setForceShowIcon(true);

        listItemMenu.inflate(R.menu.process_popup_menu);
        listItemMenu.setOnMenuItemClickListener((menuItem) -> {
            int itemId = menuItem.getItemId();
            if (itemId == R.id.process_affinity) {
                showProcessorAffinityDialog(processInfo);
            }
            else if (itemId == R.id.process_end) {
                ContentDialog.confirm(activity, R.string.do_you_want_to_end_this_process, () -> {
                    activity.getWinHandler().killProcess(processInfo.name);
                });
            }
            return true;
        });
        listItemMenu.show();
    }

    private void showProcessorAffinityDialog(final ProcessInfo processInfo) {
        ContentDialog dialog = new ContentDialog(activity, R.layout.cpu_list_dialog);
        dialog.setTitle(processInfo.name);
        dialog.setIcon(R.drawable.icon_cpu);
        final CPUListView cpuListView = dialog.findViewById(R.id.CPUListView);
        cpuListView.setCheckedCPUList(processInfo.getCPUList());
        dialog.setOnConfirmCallback(() -> {
            WinHandler winHandler = activity.getWinHandler();
            winHandler.setProcessAffinity(processInfo.pid, ProcessHelper.getAffinityMask(cpuListView.getCheckedCPUList()));
            update();
        });
        dialog.show();
    }

    private byte getMemoryUsagePercent() {
        ActivityManager activityManager = (ActivityManager)activity.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
        activityManager.getMemoryInfo(memoryInfo);
        long usedMem = memoryInfo.totalMem - memoryInfo.availMem;
        return (byte)(((double)usedMem / memoryInfo.totalMem) * 100.0f);
    }

    @Override
    public void show() {
        update();
        timer = new Timer();
        timer.schedule(new TimerTask() {
            @Override
            public void run() {
                update();
            }
        }, 0, 1000);
        super.show();
    }
}
