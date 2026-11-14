package com.thanapat.assaultcube;

import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Bundle;
import android.widget.ScrollView;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;

public class ACActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File flag = new File(getFilesDir(), "running.flag");
        String report = null;
        if (flag.exists()) {
            // the previous session did not end with a normal quit: collect what happened
            report = buildReport();
        }
        try { new FileOutputStream(flag).close(); } catch (Exception e) { }
        super.onCreate(savedInstanceState);
        if (report != null) showReport(report);
    }

    private String buildReport() {
        StringBuilder sb = new StringBuilder();
        sb.append("AssaultCube closed unexpectedly last time.\n")
          .append("Android ").append(android.os.Build.VERSION.RELEASE)
          .append(" / ").append(android.os.Build.MANUFACTURER).append(" ").append(android.os.Build.MODEL)
          .append("\n\n");
        File crash = new File(new File(getFilesDir(), "profile"), "crash.txt");
        if (crash.exists()) {
            sb.append("== native crash ==\n").append(readFile(crash)).append("\n");
            crash.delete();
        }
        sb.append("== log ==\n");
        try {
            Process p = Runtime.getRuntime().exec(new String[] { "logcat", "-d", "-v", "brief", "-t", "3000" });
            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
            java.util.ArrayDeque<String> keep = new java.util.ArrayDeque<>();
            String line;
            while ((line = r.readLine()) != null) {
                if (line.startsWith("E/") || line.startsWith("F/") || line.startsWith("W/SDL")
                        || line.contains("AssaultCube") || line.contains("SDL") || line.contains("LIBGL")
                        || line.contains("DEBUG") || line.contains("AndroidRuntime") || line.contains("libc")) {
                    keep.addLast(line);
                    if (keep.size() > 120) keep.removeFirst();
                }
            }
            for (String l : keep) sb.append(l).append('\n');
        } catch (Exception e) {
            sb.append("could not read log: ").append(e).append('\n');
        }
        String text = sb.toString();
        try {
            FileOutputStream out = new FileOutputStream(new File(getFilesDir(), "lastsession.txt"));
            out.write(text.getBytes("UTF-8"));
            out.close();
        } catch (Exception e) { }
        return text;
    }

    private static String readFile(File f) {
        try {
            FileInputStream in = new FileInputStream(f);
            byte[] buf = new byte[(int) Math.min(f.length(), 65536)];
            int n = in.read(buf);
            in.close();
            return n > 0 ? new String(buf, 0, n, "UTF-8") : "";
        } catch (Exception e) {
            return "";
        }
    }

    private void showReport(final String report) {
        TextView tv = new TextView(this);
        tv.setText(report);
        tv.setTextSize(10);
        tv.setTextIsSelectable(true);
        tv.setPadding(24, 16, 24, 16);
        ScrollView sv = new ScrollView(this);
        sv.addView(tv);
        new AlertDialog.Builder(this)
            .setTitle("AssaultCube crash report")
            .setView(sv)
            .setPositiveButton("Copy", (d, w) -> {
                ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                cm.setPrimaryClip(ClipData.newPlainText("AssaultCube report", report));
            })
            .setNegativeButton("Close", null)
            .show();
    }
}
