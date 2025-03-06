package com.f.qbdi;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.f.nativelib.NativeLib;
import com.f.qbdi.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'qbdi' library on application startup.


    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        // Example of a call to a native method
        TextView tv = binding.sampleText;
        tv.setText("test");

    }


}