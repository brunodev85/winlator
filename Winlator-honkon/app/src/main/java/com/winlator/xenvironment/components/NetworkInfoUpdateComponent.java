package com.winlator.xenvironment.components;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import com.winlator.core.FileUtils;
import com.winlator.core.NetworkHelper;
import com.winlator.xenvironment.EnvironmentComponent;

import java.io.File;

public class NetworkInfoUpdateComponent extends EnvironmentComponent {
    private BroadcastReceiver broadcastReceiver;

    @Override
    public void start() {
        Context context = environment.getContext();
        final ConnectivityManager connectivityManager = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
        final NetworkHelper networkHelper = new NetworkHelper(context);
        updateAdapterInfoFile(0, 0, 0);

        broadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                int ipAddress = 0;
                int netmask = 0;
                int gateway = 0;

                NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
                if (networkInfo != null && networkInfo.isAvailable() && networkInfo.isConnected() && networkInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                    ipAddress = networkHelper.getIpAddress();
                    netmask = networkHelper.getNetmask();
                    gateway = networkHelper.getGateway();
                }

                updateAdapterInfoFile(ipAddress, netmask, gateway);
                updateEtcHostsFile(ipAddress);
            }
        };

        IntentFilter filter = new IntentFilter();
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        context.registerReceiver(broadcastReceiver, filter);
    }

    @Override
    public void stop() {
        if (broadcastReceiver != null) {
            environment.getContext().unregisterReceiver(broadcastReceiver);
            broadcastReceiver = null;
        }
    }

    private void updateAdapterInfoFile(int ipAddress, int netmask, int gateway) {
        File file = new File(environment.getImageFs().getTmpDir(), "adapterinfo");
        FileUtils.writeString(file, "Android Wi-Fi Adapter,"+NetworkHelper.formatIpAddress(ipAddress)+","+NetworkHelper.formatNetmask(netmask)+","+NetworkHelper.formatIpAddress(gateway));
    }

    private void updateEtcHostsFile(int ipAddress) {
        String ip = ipAddress != 0 ? NetworkHelper.formatIpAddress(ipAddress) : "127.0.0.1";
        File file = new File(environment.getImageFs().getRootDir(), "etc/hosts");
        FileUtils.writeString(file, ip+"\tlocalhost\n");
    }
}
