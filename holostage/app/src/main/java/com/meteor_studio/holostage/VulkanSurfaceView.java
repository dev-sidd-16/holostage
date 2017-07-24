package com.meteor_studio.holostage;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Created by Lei on 7/18/2017.
 */

public class VulkanSurfaceView extends SurfaceView {

    public VulkanSurfaceView(Context context) {
        super(context);
        init();
    }

    private void init(){
        setZOrderOnTop(true);
        getHolder().setFormat(PixelFormat.TRANSPARENT);
    }

    public void setCallback(SurfaceHolder.Callback callback){
        getHolder().addCallback(callback);
    }

}
