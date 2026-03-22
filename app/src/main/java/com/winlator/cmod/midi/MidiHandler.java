package com.winlator.cmod.midi;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import cn.sherlock.com.sun.media.sound.SF2Soundbank;
import cn.sherlock.com.sun.media.sound.SoftSynthesizer;
import jp.kshoji.javax.sound.midi.Receiver;
import jp.kshoji.javax.sound.midi.ShortMessage;

public class MidiHandler {
    private static final String TAG = "MidiHandler";
    private DatagramSocket socket;
    private boolean running = false;
    private static final short SERVER_PORT = 7942;
    private static final short CLIENT_PORT = 7941;
    private static final int BUF_SIZE = 9;
    private final ByteBuffer receiveData = ByteBuffer.allocate(BUF_SIZE).order(ByteOrder.LITTLE_ENDIAN);
    private final DatagramPacket receivePacket = new DatagramPacket(receiveData.array(), BUF_SIZE);
    private SoftSynthesizer synth;
    private Receiver recv;
    private SF2Soundbank sf2SoundBank;
    private long lastMidiMsgTime = 0;
    private ShortMessage message = new ShortMessage();
    private ScheduledExecutorService scheduler;
    private static final long CHECK_DELAY = 200;

    public void setSoundBank(SF2Soundbank soundBank) {
        clearRecv();
        clearSynth();
        this.sf2SoundBank = soundBank;
    }

    public void start() {
        running = true;
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                socket = new DatagramSocket(null);
                socket.setReuseAddress(true);
                socket.bind(new InetSocketAddress((InetAddress) null, SERVER_PORT));

                while (running) {
                    socket.receive(receivePacket);
                    receiveData.rewind();
                    handleRequest(receiveData);

                }
            } catch (IOException e) {
            }
        });
    }

    public void stop() {
        running = false;

        if (socket != null) {
            socket.close();
            socket = null;
        }

        clearRecv();
        clearSynth();

        if (scheduler != null) {
            scheduler.shutdown();
            scheduler = null;
        }
    }

    private void handleRequest(ByteBuffer received) {
        byte requestCode = received.get();
        switch (requestCode) {
            case RequestCodes.MIDI_SHORT:
                if (recv != null) {
                    try {
                        lastMidiMsgTime = System.currentTimeMillis();
                        message.setMessage(received.get(), received.get(), received.get());
                        recv.send(message, -1);
                    } catch (Exception e) {}
                }
                break;
            case RequestCodes.MIDI_LONG:
                // FIXME: not implemented.
                break;
            case RequestCodes.MIDI_PREPARE:
                // stub
                break;
            case RequestCodes.MIDI_UNPREPARE:
                // stub
                break;
            case RequestCodes.MIDI_OPEN:
                if (synth == null || recv == null) {
                    clearRecv();
                    clearSynth();
                    prepareSynthAndRecv();
                    startMidiDataChecking();
                }
                break;
            case RequestCodes.MIDI_CLOSE:
                clearRecv();
                clearSynth();
                if (scheduler != null)
                    scheduler.shutdown();
                break;
            case RequestCodes.MIDI_RESET:
                // stub
                break;
        }
    }

    private void clearRecv() {
        if (recv != null) {
            recv.close();
            recv = null;
        }
    }

    private void clearSynth() {
        if (synth != null) {
            synth.close();
            synth = null;
        }
    }

    private void prepareSynthAndRecv() {
        try {
            synth = new SoftSynthesizer();
            synth.open();
            synth.loadAllInstruments(sf2SoundBank);
            recv = synth.getReceiver();
        } catch (Exception e) {
            clearRecv();
            clearSynth();
        }
    }

    private void sendAllOff() {
        // FIXME: A bad implement.
        if (recv != null) {
            try {
                ShortMessage msg = new ShortMessage();
                for (int i = 0; i < 128; i++) {
                    for (int j = 0; j < 16; j++) {
                        msg.setMessage(ShortMessage.NOTE_OFF, j, i, 0);
                        recv.send(msg, -1);
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public void startMidiDataChecking() {
        // FIXME: A bad implement.
        //  Since this synth doesn't supported 0xB0 0x7B 0x00 as ALL_NOTES_OFF
        if (scheduler != null)
            scheduler.shutdown();

        scheduler = Executors.newScheduledThreadPool(1);
        Runnable checkTask = () -> {
            long currentTime = System.currentTimeMillis();
            if (lastMidiMsgTime != 0 && currentTime - lastMidiMsgTime > (CHECK_DELAY /2)) {
                sendAllOff();
                lastMidiMsgTime = 0;
            }
        };
        scheduler.scheduleWithFixedDelay(checkTask, 0, CHECK_DELAY, TimeUnit.MILLISECONDS);
    }
}
