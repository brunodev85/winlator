package com.winlator.alsaserver;

import android.media.AudioFormat;
import android.media.AudioTrack;

import com.winlator.sysvshm.SysVSharedMemory;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class ALSAClient {
    public enum DataType {
        U8(1), S16LE(2), S16BE(2), FLOATLE(4), FLOATBE(4);
        public final byte byteCount;

        DataType(int byteCount) {
            this.byteCount = (byte)byteCount;
        }
    }
    private DataType dataType = DataType.U8;
    private AudioTrack audioTrack = null;
    private byte channels = 2;
    private int sampleRate = 0;
    private int position;
    private int bufferSize;
    private int frameBytes;
    private ByteBuffer buffer;

    public void release() {
        if (buffer != null) {
            SysVSharedMemory.unmapSHMSegment(buffer, buffer.capacity());
            buffer = null;
        }

        if (audioTrack != null) {
            audioTrack.pause();
            audioTrack.flush();
            audioTrack.release();
            audioTrack = null;
        }
    }

    public void prepare() {
        position = 0;
        frameBytes = channels * dataType.byteCount;
        release();

        if (!isValidBufferSize()) return;

        int encoding = AudioFormat.ENCODING_DEFAULT;
        switch (dataType) {
            case U8:
                encoding = AudioFormat.ENCODING_PCM_8BIT;
                break;
            case S16LE:
            case S16BE:
                encoding = AudioFormat.ENCODING_PCM_16BIT;
                break;
            case FLOATLE:
            case FLOATBE:
                encoding = AudioFormat.ENCODING_PCM_FLOAT;
                break;
        }

        int channelConfig = channels <= 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO;
        AudioFormat format = new AudioFormat.Builder()
            .setEncoding(encoding)
            .setSampleRate(sampleRate)
            .setChannelMask(channelConfig)
            .build();

        audioTrack = new AudioTrack.Builder()
            .setAudioFormat(format)
            .setBufferSizeInBytes(getBufferSizeInBytes())
            .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            .build();
        audioTrack.play();
    }

    public void start() {
        if (audioTrack != null && audioTrack.getPlayState() != AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.play();
        }
    }

    public void stop() {
        if (audioTrack != null) {
            audioTrack.stop();
            audioTrack.flush();
        }
    }

    public void pause() {
        if (audioTrack != null) audioTrack.pause();
    }

    public void drain() {
        if (audioTrack != null) audioTrack.flush();
    }

    public void writeDataToTrack(ByteBuffer data) {
        if (dataType == DataType.S16LE || dataType == DataType.FLOATLE) {
            data.order(ByteOrder.LITTLE_ENDIAN);
        }
        else if (dataType == DataType.S16BE || dataType == DataType.FLOATBE) {
            data.order(ByteOrder.BIG_ENDIAN);
        }

        if (audioTrack != null) {
            int bytesWritten = audioTrack.write(data, data.limit(), AudioTrack.WRITE_BLOCKING);
            if (bytesWritten > 0) position += bytesWritten;
            data.rewind();
        }
    }

    public int pointer() {
        return audioTrack != null ? position / frameBytes : 0;
    }

    public void setDataType(DataType dataType) {
        this.dataType = dataType;
    }

    public void setChannels(int channels) {
        this.channels = (byte)channels;
    }

    public void setSampleRate(int sampleRate) {
        this.sampleRate = sampleRate;
    }

    public void setBufferSize(int bufferSize) {
        this.bufferSize = bufferSize;
    }

    public ByteBuffer getBuffer() {
        return buffer;
    }

    public void setBuffer(ByteBuffer buffer) {
        this.buffer = buffer;
    }

    public DataType getDataType() {
        return dataType;
    }

    public byte getChannels() {
        return channels;
    }

    public int getSampleRate() {
        return sampleRate;
    }

    public int getBufferSize() {
        return bufferSize;
    }

    public int getBufferSizeInBytes() {
        return bufferSize * frameBytes;
    }

    private boolean isValidBufferSize() {
        return (getBufferSizeInBytes() % frameBytes == 0) && bufferSize > 0;
    }
}
