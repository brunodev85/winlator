package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.sysvshm.SysVSharedMemory;
import com.winlator.xserver.errors.BadAccess;
import com.winlator.xserver.errors.BadSHMSegment;

import java.nio.ByteBuffer;

public class SHMSegmentManager {
    private final SysVSharedMemory sysVSharedMemory;
    private final SparseArray<ByteBuffer> shmSegments = new SparseArray<>();

    public SHMSegmentManager(SysVSharedMemory sysVSharedMemory) {
        this.sysVSharedMemory = sysVSharedMemory;
    }

    public void attach(int xid, int shmid) throws BadAccess, BadSHMSegment {
        if (shmSegments.indexOfKey(xid) >= 0) detach(xid);
        ByteBuffer data = sysVSharedMemory.attach(shmid);
        if (data == null) throw new BadAccess();
        shmSegments.put(xid, data);
    }

    public void detach(int xid) throws BadSHMSegment {
        ByteBuffer data = shmSegments.get(xid);
        if (data == null) throw new BadSHMSegment(xid);
        sysVSharedMemory.detach(data);
        shmSegments.remove(xid);
    }

    public ByteBuffer getData(int xid) {
        return shmSegments.get(xid);
    }
}
