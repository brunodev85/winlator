package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.sysvshm.SysVSharedMemory;

import java.nio.ByteBuffer;

public class SHMSegmentManager {
    private final SysVSharedMemory sysVSharedMemory;
    private final SparseArray<ByteBuffer> shmSegments = new SparseArray<>();

    public SHMSegmentManager(SysVSharedMemory sysVSharedMemory) {
        this.sysVSharedMemory = sysVSharedMemory;
    }

    public void attach(int xid, int shmid) {
        if (shmSegments.indexOfKey(xid) >= 0) detach(xid);
        ByteBuffer data = sysVSharedMemory.attach(shmid);
        if (data != null) shmSegments.put(xid, data);
    }

    public void detach(int xid) {
        ByteBuffer data = shmSegments.get(xid);
        if (data != null) {
            sysVSharedMemory.detach(data);
            shmSegments.remove(xid);
        }
    }

    public ByteBuffer getData(int xid) {
        return shmSegments.get(xid);
    }
}
