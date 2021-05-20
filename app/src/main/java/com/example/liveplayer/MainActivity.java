package com.example.liveplayer;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.example.liveplayer.view.PlayerView;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    private StringBuilder stringBuilder;
    private PlayerView playerView;
    private TextView mStatus;
    private RelativeLayout mRl;

//    static { System.loadLibrary("rtsp_lib");}

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);;

        stringBuilder = new StringBuilder();
        mRl = findViewById(R.id.rl);
        mStatus = findViewById(R.id.status_tv);

        mStatus.setText(stringBuilder);
        playerView = new PlayerView(this);

        initListener();
        initdata();
//        Log.i("cyc", "onCreate: " + stringFromJNI("哈哈"));
    }

    private void initdata() {
        stringBuilder.append("网络请求成功" + "\n");
        mStatus.setText(stringBuilder);
//        playerView.setUrl("rtmp://58.200.131.2:1935/livetv/cctv1");
        playerView.setUrl("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov");
        mRl.addView(playerView);
    }

    private void initListener() {
        playerView.setBack((code, log) -> {
            this.runOnUiThread(() -> {
                stringBuilder.append(log + "\n");
                mStatus.setText(stringBuilder);
                if (code == 1) {
                    mStatus.setVisibility(View.GONE);
                }
            });
        });
    }


//    public native String stringFromJNI(String msg);
}