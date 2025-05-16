package com.winlator.winhandler;

import android.content.Context;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class MIDIHandler {
    private static final int MIDI_DATA_PORT = 7950;
    public static final short CMD_NOTE_OFF = 0x80;
    public static final short CMD_NOTE_ON = 0x90;
    public static final short CMD_KEY_PRESSURE = 0xa0;
    public static final short CMD_CONTROL_CHANGE = 0xb0;
    public static final short CMD_PROGRAM_CHANGE = 0xc0;
    public static final short CMD_PITCH_BEND = 0xe0;
    private final ByteBuffer receiveData = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN);
    private final DatagramPacket receivePacket = new DatagramPacket(receiveData.array(), 16);
    private DatagramSocket socket;
    private long nativePtr;
    private boolean opened = false;
    private ExecutorService executorService;
    private final List<Integer> midiInClients = new CopyOnWriteArrayList<>();
    private final WinHandler winHandler;
    private MidiOutputPort outputPort;
    private final MidiReceiver outputPortReceiver = new MidiReceiver() {
        @Override
        public void onSend(byte[] data, int offset, int count, long timestamp) throws IOException {
            for (int i = offset; i < count; i += 3) sendShortMsg(data[i+0], (byte)0, data[i+1], data[i+2]);
        }
    };

    public MIDIHandler(WinHandler winHandler) {
        this.winHandler = winHandler;
    }

    static {
        System.loadLibrary("midihandler");
    }

    public void outputPortConnect() {
        String selectedDevice = winHandler.activity.getPreferences().getString("midi_input_device", "auto");
        if (selectedDevice.equals("none")) return;

        MidiManager mm = (MidiManager)winHandler.activity.getSystemService(Context.MIDI_SERVICE);
        MidiDeviceInfo[] infos = mm.getDevices();

        for (MidiDeviceInfo info : infos) {
            if (info.getOutputPortCount() > 0) {
                Bundle properties = info.getProperties();
                if (selectedDevice.equals("auto") || selectedDevice.equalsIgnoreCase(properties.getString(MidiDeviceInfo.PROPERTY_NAME))) {
                    mm.openDevice(info, (device) -> {
                        synchronized (outputPortReceiver) {
                            if (device == null || outputPort != null) return;
                            outputPort = device.openOutputPort(0);
                            outputPort.connect(outputPortReceiver);
                        }
                    }, new Handler(Looper.getMainLooper()));
                    break;
                }
            }
        }
    }

    public void outputPortDisconnect() {
        synchronized (outputPortReceiver) {
            if (outputPort != null) {
                outputPort.disconnect(outputPortReceiver);
                outputPort = null;
            }
        }
    }

    public boolean init() {
        if (nativePtr != 0) return true;
        long nativePtr = nativeAllocate();
        if (nativePtr != 0) {
            this.nativePtr = nativePtr;
            return true;
        }
        else return false;
    }

    public void addClient(int port) {
        if (!midiInClients.contains(port)) midiInClients.add(port);
    }

    public boolean sendShortMsg(byte command, byte channel, byte param1, byte param2) {
        if (!winHandler.initReceived || midiInClients.isEmpty()) return false;
        final ByteBuffer sendData = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        sendData.putInt(0, (byte)(command | channel));
        sendData.put(1, param1);
        sendData.put(2, param2);

        for (final int port : midiInClients) {
            winHandler.addAction(() -> winHandler.sendPacket(port, sendData.array()));
        }

        return true;
    }

    public void open(final Runnable callback) {
        close();
        opened = true;

        executorService = Executors.newSingleThreadExecutor();
        executorService.execute(() -> {
            try {
                socket = new DatagramSocket(null);
                socket.setReuseAddress(true);
                socket.setSoTimeout(1000);
                socket.bind(new InetSocketAddress((InetAddress)null, MIDI_DATA_PORT));

                if (callback != null) callback.run();

                while (opened) {
                    try {
                        socket.receive(receivePacket);

                        receiveData.rewind();
                        boolean isShortData = receiveData.get() == 1;
                        processData(receiveData.get(), receiveData.get(), receiveData.get());
                    }
                    catch (SocketTimeoutException e) {}
                }
            }
            catch (IOException e) {}
        });
    }

    public void close() {
        midiInClients.clear();
        if (!opened) return;
        opened = false;

        if (executorService != null) {
            try {
                executorService.awaitTermination(2, TimeUnit.SECONDS);
            }
            catch (InterruptedException e) {}
            executorService = null;
        }

        if (socket != null) {
            socket.close();
            socket = null;
        }
    }

    private void processData(byte status, byte param1, byte param2) {
        int command = (status & 0xf0);
        int channel = (status & 0x0f);

        switch (command) {
            case CMD_NOTE_OFF:
                noteOff(nativePtr, channel, param1);
                break;
            case CMD_NOTE_ON:
                noteOn(nativePtr, channel, param1, param2);
                break;
            case CMD_KEY_PRESSURE:
                keyPressure(nativePtr, channel, param1, param2);
                break;
            case CMD_CONTROL_CHANGE:
                controlChange(nativePtr, channel, param1, param2);
                break;
            case CMD_PROGRAM_CHANGE:
                programChange(nativePtr, channel, param1);
                break;
            case CMD_PITCH_BEND:
                pitchBend(nativePtr, channel, param1 + (param2 << 7));
                break;
        }
    }

    public void noteOn(int channel, int note, int velocity) {
        noteOn(nativePtr, channel, note, velocity);
    }

    public void noteOff(int channel, int note) {
        noteOff(nativePtr, channel, note);
    }

    public void loadSoundFont(String soundfontPath) {
        loadSoundFont(nativePtr, soundfontPath);
    }

    public void programChange(int channel, int program) {
        programChange(nativePtr, channel, program);
    }

    public void destroy() {
        if (nativePtr != 0) {
            destroy(nativePtr);
            nativePtr = 0;
        }
    }

    private native long nativeAllocate();

    private native void destroy(long nativePtr);

    private native void noteOn(long nativePtr, int channel, int note, int velocity);

    private native void noteOff(long nativePtr, int channel, int note);

    private native void keyPressure(long nativePtr, int channel, int key, int value);

    private native void programChange(long nativePtr, int channel, int program);

    private native void controlChange(long nativePtr, int channel, int control, int value);

    private native void pitchBend(long nativePtr, int channel, int value);

    private native void loadSoundFont(long nativePtr, String soundfontPath);

    public static String[] getInstrumentNames() {
        return new String[]{"Acoustic Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano", "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavi", "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer", "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica", "Tango Accordion", "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)", "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar harmonics", "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", "Violin", "Viola", "Cello", "Contrabass", "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani", "String Ensemble 1", "String Ensemble 2", "SynthStrings 1", "SynthStrings 2", "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit", "Trumpet", "Trombone", "Tuba", "Muted Trumpet", "French Horn", "Brass Section", "SynthBrass 1", "SynthBrass 2", "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", "Oboe", "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute", "Recorder", "Pan Flute", "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina", "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)", "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)", "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)", "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)", "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)", "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Bag pipe", "Fiddle", "Shanai", "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal", "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter", "Applause", "Gunshot"};
    }

    public static String[] getNotes() {
        byte octaves = 6;
        String[] symbols = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        String[] notes = new String[symbols.length * octaves];

        int index = 0;
        for (byte i = 1; i <= octaves; i++) {
            for (String symbol : symbols) notes[index++] = symbol+i;
        }

        return notes;
    }

    public static int parseNoteNumber(String note) {
        Pattern pattern = Pattern.compile("^([A-Za-z#]+)([0-9]{1})$");
        Matcher matcher = pattern.matcher(note);

        int octave = 1;
        if (matcher.find()) {
            note = matcher.group(1);
            octave = Integer.parseInt(matcher.group(2));
        }

        int offset = (octave - 1) * 12;
        switch (note) {
            case "C":
                return 0 + offset;
            case "C#":
            case "Db":
                return 1 + offset;
            case "D":
                return 2 + offset;
            case "D#":
            case "Eb":
                return 3 + offset;
            case "E":
                return 4 + offset;
            case "F":
                return 5 + offset;
            case "F#":
            case "Gb":
                return 6 + offset;
            case "G":
                return 7 + offset;
            case "G#":
            case "Ab":
                return 8 + offset;
            case "A":
                return 9 + offset;
            case "A#":
            case "Bb":
                return 10 + offset;
            case "B":
                return 11 + offset;
            default:
                return -1;
        }
    }
}
