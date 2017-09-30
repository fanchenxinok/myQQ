/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include <gst/rtsp-server/rtsp-server.h>
#include <getopt.h>

#define PORT "8554"
#define MIN_HOST_IP "172.26.190.0"
#define MAX_HOST_IP "172.26.190.255"
#define SERVER_IP "172.26.190.135"

typedef struct{
    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMediaFactory *factory;
}rtsp_info_st;

static gboolean timeout (GstRTSPServer * server, gboolean ignored)
{
  GstRTSPSessionPool *pool;

  pool = gst_rtsp_server_get_session_pool (server);
  gst_rtsp_session_pool_cleanup (pool);
  g_object_unref (pool);

  return TRUE;
}

static void remove_map (GstRTSPServer * server)
{
    GstRTSPMountPoints *mounts;

    g_print ("removing /test mount point\n");
    mounts = gst_rtsp_server_get_mount_points (server);
    gst_rtsp_mount_points_remove_factory (mounts, "/test");
    g_object_unref (mounts);

    return;
}

static void add_map (rtsp_info_st *pRtspInfo)
{
    GstRTSPMountPoints *mounts;

    g_print ("adding /test mount point\n");
    mounts = gst_rtsp_server_get_mount_points (pRtspInfo->server);
    gst_rtsp_mount_points_add_factory (mounts, "/test", pRtspInfo->factory);
    /* don't need the ref to the mapper anymore */
    g_object_unref (mounts);

    return;
}

static void media_configure (
    GstRTSPMediaFactory * factory,
    GstRTSPMedia * media,
    gpointer user_data)
{
    gst_rtsp_media_suspend(media);
    GstElement *element;

    /* get the element used for providing the streams of the media */
    element = gst_rtsp_media_get_element (media);

    /* get our appsrc, we named it 'mysrc' with the name property */
    GstElement *pVideoSrc = NULL;
    pVideoSrc = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc");

    if(pVideoSrc == NULL)
        g_print("pVideoSrc == NULL\n");

    static gboolean flag = FALSE;
    if(flag == FALSE){
        /* configure the caps of the video */
        #if 0
        g_object_set (G_OBJECT (pVideoSrc), "caps",
            gst_caps_new_simple ("video/x-raw",
            "width", G_TYPE_INT, 360,
            "height", G_TYPE_INT, 240, NULL), NULL);
        flag = TRUE;
        g_print("...media_configure()-->(360,240)\n");
        #else
        g_object_set (G_OBJECT (pVideoSrc), "pattern", "ball", NULL);
        g_print("...media_configure()-->BALL\n");
        flag = TRUE;
        #endif
    }
    else{
        /* configure the caps of the video */
        g_object_set (G_OBJECT (pVideoSrc), "pattern", "snow", NULL);
        g_print("...media_configure()-->SNOW\n");
        flag = FALSE;
    }
    gst_rtsp_media_unsuspend(media);
    return;
}


static gboolean timeout_change_videosize(gpointer user_data)
{
  rtsp_info_st *pRtspInfo = (rtsp_info_st*)user_data;
  //GstRTSPMediaFactory *factory = (GstRTSPMediaFactory*)user_data;
  //g_main_loop_quit(pRtspInfo->loop);
  static gboolean flag = FALSE;
  int width = 0, height = 0;
  gchar *str = NULL;
  //remove_map(pRtspInfo->server);

  #if 1
  //gst_rtsp_media_set_pipeline_state(pRtspInfo->factory, GST_STATE_NULL);
  //g_print("set state is NULL\n");
  if(flag == FALSE){
        width = 360;
        height = 240;
        str = g_strdup_printf ("( "
              "videotestsrc pattern=ball ! video/x-raw,width=%d,height=%d,framerate=60/1 ! "
              "avenc_mpeg2video ! rtpmpvpay name=pay0 pt=96 "
              "audiotestsrc ! avenc_aac compliance=-2 "
              "! rtpmp4apay name=pay1 pt=97 " ")", width, height);
          gst_rtsp_media_factory_set_launch (pRtspInfo->factory, str);
          g_free(str);
          str = NULL;
     
        //add_map(pRtspInfo);
        g_print("...timeout_change_videosize()-->(%d,%d)\n",width,height);
        flag = TRUE;
  }
  else{
        width = 720;
        height = 480;

        str = g_strdup_printf ("( "
          "videotestsrc ! video/x-raw,width=%d,height=%d,framerate=60/1 ! "
          "avenc_mpeg2video ! rtpmpvpay name=pay0 pt=96 "
          "audiotestsrc ! avenc_aac compliance=-2 "
          "! rtpmp4apay name=pay1 pt=97 " ")", width, height);
        gst_rtsp_media_factory_set_launch (pRtspInfo->factory, str);
        g_free(str);
        str = NULL;
     
        //add_map(pRtspInfo);
        g_print("...timeout_change_videosize()-->(%d,%d)\n",width,height);
        flag = FALSE;
  }
  
  //gst_rtsp_media_set_pipeline_state(pRtspInfo->factory, GST_STATE_PLAYING);
  //g_print("set state is PLAYING\n");
  //gst_rtsp_media_factory_set_shared (pRtspInfo->factory, TRUE);
  //g_main_loop_run(pRtspInfo->loop);
  #else
  g_print("timeout_change_videosize()..........\n");
  g_signal_connect (pRtspInfo->factory,
            "media-configure",
            (GCallback)media_configure, NULL);
  #endif
  return TRUE;
}


int main (int argc, char *argv[])
{
    if(argc != 1 && argc != 3
            && argc != 5 && argc != 7){
            printf("invalide args, usage:  ./rtsp_sender " \
                "[-p[--port] port number] "\
                "[-m[--minip] the min host ip address of client's] "\
                "[-M[--maxip] the max host ip address of client's]\n");
            return -1;
        }
        
        char *short_options = "p:m:M:"; 
        static struct option long_options[] = {  
           //{"reqarg", required_argument, NULL, 'r'},  
           //{"noarg",  no_argument,       NULL, 'n'},  
           //{"optarg", optional_argument, NULL, 'o'}, 
           {"port", required_argument, NULL, 'p'+'l'},
           {"minip", required_argument, NULL, 'm'+'l'},
           {"maxip", required_argument, NULL, 'M'+'l'},
           {0, 0, 0, 0}  
       };  
    
        int opt = 0;
        char port[10] = {'\0'};
        char minIp[30] = {'\0'};
        char maxIp[30] = {'\0'};
    
        while ( (opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1){
            //printf("opt = %d\n", opt);
            switch(opt){
                case 'p':
                case 'p'+'l':
                    if(optarg){
                        char* pPort = optarg;
                        int len = strlen(pPort);
                        strncpy(port, pPort, len);
                        port[len] = '\0';
                        printf("port number = %s\n", port);
                    }
                    else
                        printf("port number is null\n");
                    break;
                case 'm':
                case 'm'+'l':
                    if(optarg){
                        char* pMinIp = optarg;
                        int len = strlen(pMinIp);
                        strncpy(minIp, pMinIp, len);
                        minIp[len] = '\0';
                        printf("min host ip = %s\n", minIp);
                    }
                    else
                        printf("min host ip optarg is null\n");
                    break;
                case 'M':
                case 'M'+'l':
                    if(optarg){
                        char* pMaxIp = optarg;
                        int len = strlen(pMaxIp);
                        strncpy(maxIp, pMaxIp, len);
                        maxIp[len] = '\0';
                        printf("max host ip = %s\n", maxIp);
                    }
                    else
                        printf("max host ip optarg is null\n");
                    break;
                default:
                    break;
            }
        }

    rtsp_info_st rtspInfo;
    //GMainLoop *loop;
    GstRTSPMountPoints *mounts;
    GstRTSPAddressPool *pool;

    gst_init (&argc, &argv);

    rtspInfo.loop = g_main_loop_new (NULL, FALSE);

    /* create a server instance */
    rtspInfo.server = gst_rtsp_server_new ();
    if(port[0] != '\0'){
        g_object_set (rtspInfo.server, "service", port, NULL);
        g_print("server set port is:%s\n", port);
    }

    /* get the mount points for this server, every server has a default object
    * that be used to map uri mount points to media factories */
    mounts = gst_rtsp_server_get_mount_points (rtspInfo.server);

    /* make a media factory for a test stream. The default media factory can use
    * gst-launch syntax to create pipelines. 
    * any launch line works as long as it contains elements named pay%d. Each
    * element with pay%d names will be a stream */
    rtspInfo.factory = gst_rtsp_media_factory_new ();
    gst_rtsp_media_factory_set_launch (rtspInfo.factory, "( "
#if 1
      "videotestsrc name=videosrc pattern=ball ! video/x-raw,width=720,height=480,framerate=60/1 ! "
      "avenc_mpeg2video ! rtpmpvpay name=pay0 pt=96 "
      "audiotestsrc ! avenc_aac compliance=-2 "
      "! rtpmp4apay name=pay1 pt=97 " ")");
#else
    "videotestsrc pattern=ball ! video/x-raw,width=720,height=480,framerate=60/1 ! "
      "tee name=videotee videotee. ! avenc_mpeg2video ! rtpmpvpay name=pay0 pt=96 "
      "videotee. ! videoconvert ! ximagesink "
      "audiotestsrc ! avenc_aac compliance=-2 "
      "! rtpmp4apay name=pay1 pt=97 " ")");
#endif

    gst_rtsp_media_factory_set_shared (rtspInfo.factory, TRUE);

    /* make a new address pool */
    pool = gst_rtsp_address_pool_new ();
    
    gst_rtsp_address_pool_add_range (pool,MIN_HOST_IP, MAX_HOST_IP, 5000, 8888, 1);
    gst_rtsp_media_factory_set_address_pool (rtspInfo.factory, pool);
    g_object_unref (pool);

    /* attach the test factory to the /test url */
    gst_rtsp_mount_points_add_factory (mounts, "/test", rtspInfo.factory);

    /* don't need the ref to the mapper anymore */
    g_object_unref (mounts);

    /* attach the server to the default maincontext */
    if (gst_rtsp_server_attach (rtspInfo.server, NULL) == 0)
        goto failed;

    g_timeout_add_seconds (2, (GSourceFunc) timeout, rtspInfo.server);
    //g_timeout_add_seconds (5, timeout_change_videosize, &rtspInfo);
    //g_timeout_add_seconds (5, timeout_state_control_cb, &video_audio_p);
    /* start serving */
    g_print ("stream ready at rtsp://%s:%s/test\n",SERVER_IP, (port[0] == '\0') ? PORT : port);
    //gst_rtsp_media_set_pipeline_state(rtspInfo.factory, GST_STATE_NULL);
    //g_print("----------------------\n");
    g_main_loop_run (rtspInfo.loop);

    return 0;

    /* ERRORS */
    failed:
    {
        g_print ("failed to attach the server\n");
        return -1;
    }
}
