#include <stdio.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include <gst/gst.h>
#define HOST_IP "172.26.178.91"
#define VIDEO_RTP_DEPAY "rtpmpvdepay"
#define AUDIO_RTP_DEPAY "rtpmp4adepay"
#define VIDEO_PARSE "mpegvideoparse"
#define AUDIO_PARSE "aacparse"
#define RTSP_LOCATION "rtsp://127.0.0.1:8554/test"
#define PORT 8888
#define ALSA_DEVICE "alsa_output.pci-0000_00_05.0.analog-stereo.monitor"

#define CHANGE_VIDEO_SIZE (1)  /* 需要变更video 的高宽 */

/* 终端执行的命令:

/*
gst-launch-1.0 -v rtspsrc name=rtspsrc location=rtsp://172.26.178.91:8554/test 
rtspsrc. ! rtpmpvdepay ! decodebin ! videoconvert ! ximagesink 
rtspsrc. ! rtpmp4adepay ! decodebin ! audioconvert ! 
"audio/x-raw,layout=(string)interleaved,rate=(int)44100,channels=(int)2" ! autoaudiosink
*/

/* 编译
gcc -Wall rtsp_receiver.c -o rtsp_receiver $(pkg-config --cflags --libs gstreamer-1.0)
*/

typedef enum{
    MSC_FAIL = -1,
    MSC_OK = 0,
    MSC_INVALID_PRAM,
}MSC_RES;

#define checkIsNull(p) \
    do{\
        if(p == NULL){\
            g_print("check NULL pointer then return, line = %d\n", __LINE__);\
            return MSC_FAIL;\
        }\
    }while(0)

typedef struct{
    GstElement *pPipeline;
    GstElement *pRtspSrc;
    GstElement *pRtpdepay;
    //GstElement *pParse;
    GstElement *pDecodebin;
    GstElement *pConvert;
    #if CHANGE_VIDEO_SIZE
    GstElement *pVideoScale;
    GstElement *pVideoCapsFilter;
    #endif
    GstElement *pSink;
}streamline_st; 

typedef struct{
    GstElement *pVideoPipeline;
    GstElement *pAudioPipeline;
}video_audio_pipeline_st;

static gboolean link_elements_with_filter_simple (GstElement *element1, GstElement *element2)
{
  gboolean link_ok;
  GstCaps *caps;

  caps = gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "I420",
          "width", G_TYPE_INT, 384,
          "height", G_TYPE_INT, 288,
          "framerate", GST_TYPE_FRACTION, 25, 1,
          NULL);

  link_ok = gst_element_link_filtered (element1, element2, caps);
  gst_caps_unref (caps);

  if (!link_ok) {
    g_warning ("Failed to link element1 and element2!");
  }

  return link_ok;
}

static gboolean link_elements_with_filter (GstElement *element1, GstElement *element2)
{
    gboolean link_ok;
    GstCaps *caps;

    caps = gst_caps_new_full (
      gst_structure_new ("video/x-raw",
             "width", G_TYPE_INT, 384,
             "height", G_TYPE_INT, 288,
             "framerate", GST_TYPE_FRACTION, 25, 1,
             NULL),
      gst_structure_new ("video/x-bayer",
             "width", G_TYPE_INT, 384,
             "height", G_TYPE_INT, 288,
             "framerate", GST_TYPE_FRACTION, 25, 1,
             NULL),
      NULL);

    link_ok = gst_element_link_filtered (element1, element2, caps);
    gst_caps_unref (caps);

    if (!link_ok) {
        g_warning ("Failed to link element1 and element2!");
    }

    return link_ok;
}


static void set_filter_caps_simple(GstElement *capsfilter)
{
  GstCaps *caps;

  caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 720,
          "height", G_TYPE_INT, 480,
          NULL);

  g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL); 
  gst_caps_unref (caps);

  return;
}

static void set_filter_caps(GstElement *capsfilter, GstCaps *caps)
{
  g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL); 
  gst_caps_unref (caps);

  return;
}


static void cb_link_rtspsrc_pad(GstElement *element, GstPad* pad, gpointer data)
{
    gchar *name;
    GstCaps * p_caps;
    gchar * description;
    GstElement *pRtpDepay;

    name = gst_pad_get_name(pad);
    g_print("A new pad %s was created\n", name);

    // here, you would setup a new pad link for the newly created pad
    // sooo, now find that rtph264depay is needed and link them?
    p_caps = gst_pad_get_pad_template_caps (pad);

    description = gst_caps_to_string(p_caps);
    g_print("%s\n", description);
    g_free(description);

    pRtpDepay = GST_ELEMENT(data);

    // try to link the pads then ...
    if(!gst_element_link_pads(element, name, pRtpDepay, "sink")){
        g_print("------cb_link_rtspsrc_pad(), Failed to link elements pad\n");
    }

    g_free(name);
}

static void add_pad (GstElement *element , GstPad *pad , gpointer data)
{
    gchar *name;
    GstElement *sink = (GstElement*)data;

    name = gst_pad_get_name(pad);
    if(!gst_element_link_pads(element , name , sink , "sink")){
       g_print("add_pad(), Failed to link elements pad\n"); 
    }
    g_free(name);
}

/* 定时设置视频的高宽 */
static gboolean timeout_videosize_change_cb(gpointer user_data)
{
    streamline_st *pVideoStreamLine = (streamline_st*)user_data;
    /* 如果发送端的视频高宽小于变化的最大值就会
    挂掉，需要重新把pipeline 状态设置为NULL*/
    //gst_element_set_state (pVideoStreamLine->pPipeline, GST_STATE_NULL);
    GstCaps *caps;
    static gboolean flag = FALSE;
    int width = 0, height = 0;
    if(flag != FALSE){
        width = 720;
        height = 480;
        flag = FALSE;
    }
    else{
        width = 360;
        height = 240;
        flag = TRUE;
    }
    g_print("#### timeout_cb()-->change video to [%d, %d]\n", width, height);
    
    caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, width,
          "height", G_TYPE_INT, height,
          NULL);

    g_object_set(G_OBJECT(pVideoStreamLine->pVideoCapsFilter), "caps", caps, NULL); 
    gst_caps_unref (caps);

    //gst_element_set_state (pVideoStreamLine->pPipeline, GST_STATE_PLAYING);
    return TRUE;
}

/* media share play client init */
MSC_RES MSC_Init(GMainLoop **loop)
{
    gst_init(NULL, NULL);
    *loop = g_main_loop_new(NULL, FALSE);
    checkIsNull(loop);
    return MSC_OK;
}

MSC_RES MSC_CreateVideoStreamLine(
    streamline_st *pVideoStreamLine,
    const char* pRtspLocation,
    const char* pVideoRtpDepay)
{
    /* video pipe line */
    pVideoStreamLine->pPipeline = gst_pipeline_new(NULL);
    g_assert(pVideoStreamLine->pPipeline);

    pVideoStreamLine->pRtspSrc = gst_element_factory_make("rtspsrc","video_rtspsrc");
    g_assert(pVideoStreamLine->pRtspSrc);
    if(pRtspLocation == NULL){
        g_object_set(G_OBJECT(pVideoStreamLine->pRtspSrc),"location", RTSP_LOCATION, "latency", 100, NULL);
        g_print("Rtsp location: %s\n", RTSP_LOCATION);
    }
    else{
        g_object_set(G_OBJECT(pVideoStreamLine->pRtspSrc),"location", pRtspLocation, "latency", 100, NULL);
        g_print("Rtsp location: %s\n", pRtspLocation);
    }

    if(pVideoRtpDepay == NULL){
        pVideoStreamLine->pRtpdepay = gst_element_factory_make(VIDEO_RTP_DEPAY,"video_rtp_depay");
    }
    else{
        pVideoStreamLine->pRtpdepay = gst_element_factory_make(pVideoRtpDepay,"video_rtp_depay");
    }
    g_assert(pVideoStreamLine->pRtpdepay);

    //pVideoStreamLine->pParse = gst_element_factory_make(VIDEO_PARSE,"video_parse");
    //g_assert(pVideoStreamLine->pParse);
       
    pVideoStreamLine->pDecodebin = gst_element_factory_make("decodebin","video_decodebin");
    g_assert(pVideoStreamLine->pDecodebin);

    pVideoStreamLine->pConvert = gst_element_factory_make("videoconvert","video_convert");
    g_assert(pVideoStreamLine->pConvert);

    pVideoStreamLine->pSink = gst_element_factory_make("ximagesink","video_sink");
    g_assert(pVideoStreamLine->pSink);

    gst_bin_add_many(GST_BIN(pVideoStreamLine->pPipeline),
        pVideoStreamLine->pRtspSrc, 
        pVideoStreamLine->pRtpdepay, 
        pVideoStreamLine->pDecodebin,
        pVideoStreamLine->pConvert,
        pVideoStreamLine->pSink, NULL);
    
    g_signal_connect(pVideoStreamLine->pRtspSrc,
        "pad-added",
        G_CALLBACK(cb_link_rtspsrc_pad), 
        pVideoStreamLine->pRtpdepay);

    if(!gst_element_link_many(pVideoStreamLine->pRtpdepay, pVideoStreamLine->pDecodebin, NULL)){
        g_error("Failed to link pVideoRtpdepay->pVideoDecodebin \n");
    }
    
    g_signal_connect(pVideoStreamLine->pDecodebin, 
        "pad-added" , 
        G_CALLBACK(add_pad) ,
        pVideoStreamLine->pConvert);

#if CHANGE_VIDEO_SIZE
    pVideoStreamLine->pVideoScale = gst_element_factory_make("videoscale","video_scale");
    g_assert(pVideoStreamLine->pVideoScale);

    pVideoStreamLine->pVideoCapsFilter = gst_element_factory_make("capsfilter","video_capsfilter");
    g_assert(pVideoStreamLine->pVideoCapsFilter);

    gst_bin_add_many(GST_BIN(pVideoStreamLine->pPipeline),
        pVideoStreamLine->pVideoScale,
        pVideoStreamLine->pVideoCapsFilter, NULL);

    GstCaps *caps;
    caps = gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, 720,
          "height", G_TYPE_INT, 480,
          NULL);

    g_object_set(G_OBJECT(pVideoStreamLine->pVideoCapsFilter), "caps", caps, NULL); 
    gst_caps_unref (caps);

    if(!gst_element_link_many(pVideoStreamLine->pConvert,
        pVideoStreamLine->pVideoScale,
        pVideoStreamLine->pVideoCapsFilter,
        pVideoStreamLine->pSink,NULL)){
        g_error("Failed to link video pipeline \n");
        return MSC_FAIL;
    }
#else
    if(!gst_element_link_many(pVideoStreamLine->pConvert,pVideoStreamLine->pSink,NULL)){
        g_error("Failed to link video pipeline \n");
        return MSC_FAIL;
    }
#endif
    
    return MSC_OK;
}

MSC_RES MSC_CreateAudioStreamLine(
    streamline_st *pAudioStreamLine,
    const char* pRtspLocation,
    const char* pAudioRtpDepay)
{
    pAudioStreamLine->pPipeline = gst_pipeline_new(NULL);
    g_assert(pAudioStreamLine->pPipeline);

    pAudioStreamLine->pRtspSrc = gst_element_factory_make("rtspsrc","audio_rtspsrc");
    g_assert(pAudioStreamLine->pRtspSrc);
    if(pRtspLocation == NULL){
        g_object_set(G_OBJECT(pAudioStreamLine->pRtspSrc),"location", RTSP_LOCATION, "latency", 100, NULL);
        g_print("Rtsp location: %s\n", RTSP_LOCATION);
    }
    else{
        g_object_set(G_OBJECT(pAudioStreamLine->pRtspSrc),"location", pRtspLocation, "latency", 100, NULL);
        g_print("Rtsp location: %s\n", pRtspLocation);
    }
    
    if(pAudioRtpDepay == NULL){
        pAudioStreamLine->pRtpdepay = gst_element_factory_make(AUDIO_RTP_DEPAY,"audio_rtp_depay");
    }
    else{
        pAudioStreamLine->pRtpdepay = gst_element_factory_make(pAudioRtpDepay,"audio_rtp_depay");
    }
    g_assert(pAudioStreamLine->pRtpdepay);
       
    pAudioStreamLine->pDecodebin = gst_element_factory_make("decodebin","audio_decodebin");
    g_assert(pAudioStreamLine->pDecodebin);

    pAudioStreamLine->pConvert = gst_element_factory_make("audioconvert","audio_convert");
    g_assert(pAudioStreamLine->pConvert);

    pAudioStreamLine->pSink = gst_element_factory_make("autoaudiosink","audio_sink");
    g_assert(pAudioStreamLine->pSink);

    gst_bin_add_many(GST_BIN(pAudioStreamLine->pPipeline),
        pAudioStreamLine->pRtspSrc,
        pAudioStreamLine->pRtpdepay,
        pAudioStreamLine->pDecodebin,
        pAudioStreamLine->pConvert,
        pAudioStreamLine->pSink, NULL);

    g_signal_connect(pAudioStreamLine->pRtspSrc,
        "pad-added", 
        G_CALLBACK(cb_link_rtspsrc_pad), 
        pAudioStreamLine->pRtpdepay);

    if(!gst_element_link_many(pAudioStreamLine->pRtpdepay, pAudioStreamLine->pDecodebin, NULL)){
        g_error("Failed to link pAudioRtpdepay->pAudioDecodebin \n");
    }
    
    g_signal_connect(pAudioStreamLine->pDecodebin,
        "pad-added" ,
        G_CALLBACK(add_pad),
        pAudioStreamLine->pConvert);

    if(!gst_element_link_many(pAudioStreamLine->pConvert,pAudioStreamLine->pSink,NULL)){
        g_error("Failed to link Audio pipeline \n");
        return MSC_FAIL;
    }

    return MSC_OK;
}

MSC_RES MSC_PlayStart(
    GMainLoop *loop,
    GstElement *pVideoPipeline,
    GstElement *pAudioPipeline)
{
    checkIsNull(loop);
    gst_element_set_state (pVideoPipeline, GST_STATE_PLAYING);
    gst_element_set_state (pAudioPipeline, GST_STATE_PLAYING);
    g_print("g_main_loop_run........\n");
    g_main_loop_run(loop);
    return MSC_OK;
}

void MSC_PlayStop(GstElement *pVideoPipeline, GstElement *pAudioPipeline)
{
    gst_element_set_state (pVideoPipeline, GST_STATE_NULL);
    gst_element_set_state (pAudioPipeline, GST_STATE_NULL);
    return;
}

static void client_setPlaying(GstElement *pVideoPipeline, GstElement *pAudioPipeline)
{
    gst_element_set_state (pVideoPipeline, GST_STATE_PLAYING);
    gst_element_set_state (pAudioPipeline, GST_STATE_PLAYING);
    return;
}

static void client_setPaused(GstElement *pVideoPipeline, GstElement *pAudioPipeline)
{
    gst_element_set_state (pVideoPipeline, GST_STATE_PAUSED);
    gst_element_set_state (pAudioPipeline, GST_STATE_PAUSED);
    return;
}

static void client_setReady(GstElement *pVideoPipeline, GstElement *pAudioPipeline)
{
    gst_element_set_state (pVideoPipeline, GST_STATE_READY);
    gst_element_set_state (pAudioPipeline, GST_STATE_READY);
    return;
}

static void client_setStop(GstElement *pVideoPipeline, GstElement *pAudioPipeline)
{
    gst_element_set_state (pVideoPipeline, GST_STATE_NULL);
    gst_element_set_state (pAudioPipeline, GST_STATE_NULL);
    return;
}

static gboolean timeout_state_control_cb(gpointer user_data)
{
    video_audio_pipeline_st *pStreamLine = (video_audio_pipeline_st*)user_data;

    //gst_element_set_state (pVideoStreamLine->pPipeline, GST_STATE_NULL);
    GstCaps *caps;
    static gboolean flag = FALSE;
    int width = 0, height = 0;
    if(flag != FALSE){
        client_setPaused(pStreamLine->pVideoPipeline, pStreamLine->pAudioPipeline);
        g_print("$$$$$$$$ client_state_control_cb()-->change state to PAUSED\n");
        flag = FALSE;
    }
    else{
        client_setPlaying(pStreamLine->pVideoPipeline, pStreamLine->pAudioPipeline);
        g_print("$$$$$$$$ client_state_control_cb()-->change state to PLAYING\n");
        flag = TRUE;
    }
    
    return TRUE;
}


int main(int argc,char *argv[])
{
    if(argc != 1 && argc != 3
        && argc != 5 && argc != 7){
        printf("invalide args, usage:  ./music_server " \
            "[-v[--vrtpdepay] video rtpdepay] "\
            "[-l[--location] rtspsrc location] "\
            "[-a[--artpdepay] audio rtpdepay]\n");
        return -1;
    }
    
    char *short_options = "v:a:l:"; 
    static struct option long_options[] = {  
       //{"reqarg", required_argument, NULL, 'r'},  
       //{"noarg",  no_argument,       NULL, 'n'},  
       //{"optarg", optional_argument, NULL, 'o'}, 
       {"vrtpdepay", required_argument, NULL, 'v'+'l'},
       {"location", required_argument, NULL, 'l'+'l'},
       {"artpdepay", required_argument, NULL, 'a'+'l'},
       {0, 0, 0, 0}  
   };  

    int opt = 0;
    char video_rtp_depay[24] = {'\0'};
    char audio_rtp_depay[24] = {'\0'};
    char rtsp_location[256] = {'\0'};

    while ( (opt = getopt_long(argc, argv, short_options, long_options, NULL)) != -1){
        //printf("opt = %d\n", opt);
        switch(opt){
            case 'v':
            case 'v'+'l':
                if(optarg){
                    char* depay = optarg;
                    int len = strlen(depay);
                    strncpy(video_rtp_depay, depay, len);
                    video_rtp_depay[len] = '\0';
                    printf("video rtp depay name = %s\n", video_rtp_depay);
                }
                else
                    printf("video rtp depay name optarg is null\n");
                break;
            case 'l':
            case 'l'+'l':
                if(optarg){
                    char* location = optarg;
                    int len = strlen(location);
                    strncpy(rtsp_location, location, len);
                    rtsp_location[len] = '\0';
                    printf("rtsp location = %s\n", rtsp_location);
                }
                else
                    printf("rtsp location optarg is null\n");
                break;
            case 'a':
            case 'a'+'l':
                if(optarg){
                    char* depay = optarg;
                    int len = strlen(depay);
                    strncpy(audio_rtp_depay, depay, len);
                    audio_rtp_depay[len] = '\0';
                    printf("audio rtp depay name = %s\n", audio_rtp_depay);
                }
                else
                    printf("audio rtp depay name optarg is null\n");
                break;
            default:
                break;
        }
    }

    /*
        gst-launch-1.0 -v rtspsrc name=rtspsrc location=rtsp://172.26.178.91:8554/test 
        rtspsrc. ! rtpmpvdepay ! decodebin ! videoconvert ! ximagesink 
        rtspsrc. ! rtpmp4adepay ! decodebin ! audioconvert ! autoaudiosink
    */

#if 0
    GstElement *pVideoRtpdepay,*pVideoParse, *pVideoDecodebin,*pVideoConvert,*pVideoSink;
    GstElement *pAudioRtpdepay,*pAudioParse, *pAudioDecodebin,*pAudioConvert,*pAudioSink;	
    GstElement *pVideoPipeline, *pAudioPipeline, *pVideoRtspSrc, *pAudioRtspSrc;
    GMainLoop *loop = NULL;
    GstPad *rtspsrcpad,*sinkpad;

    //gst_init(&argc, &argv);
    gst_init(NULL, NULL);

    /* video pipe line */
    pVideoPipeline = gst_pipeline_new(NULL);
    g_assert(pVideoPipeline);

    pVideoRtspSrc = gst_element_factory_make("rtspsrc","video_rtspsrc");
    g_assert(pVideoRtspSrc);
    if(rtsp_location[0] == '\0'){
        g_object_set(G_OBJECT(pVideoRtspSrc),"location", RTSP_LOCATION, "latency", 100, NULL);
        printf("Rtsp location: %s\n", RTSP_LOCATION);
    }
    else{
        g_object_set(G_OBJECT(pVideoRtspSrc),"location", rtsp_location, "latency", 100, NULL);
        printf("Rtsp location: %s\n", rtsp_location);
    }

    if(video_rtp_depay[0] == '\0'){
        pVideoRtpdepay = gst_element_factory_make(VIDEO_RTP_DEPAY,"video_rtp_depay");
        g_assert(pVideoRtpdepay);
    }
    else{
        pVideoRtpdepay = gst_element_factory_make(video_rtp_depay,"video_rtp_depay");
        g_assert(pVideoRtpdepay);
    }

    //pVideoParse = gst_element_factory_make(VIDEO_PARSE,"video_parse");
    //g_assert(pVideoParse);
       
    pVideoDecodebin = gst_element_factory_make("decodebin","video_decodebin");
    g_assert(pVideoDecodebin);

    pVideoConvert = gst_element_factory_make("videoconvert","video_convert");
    g_assert(pVideoConvert);

    pVideoSink = gst_element_factory_make("ximagesink","video_sink");
    g_assert(pVideoSink);

    gst_bin_add_many(GST_BIN(pVideoPipeline), pVideoRtspSrc, pVideoRtpdepay, pVideoDecodebin,pVideoConvert,pVideoSink,NULL);
    g_signal_connect(pVideoRtspSrc, "pad-added", G_CALLBACK(cb_link_rtspsrc_pad), pVideoRtpdepay);

    if(!gst_element_link_many(pVideoRtpdepay, pVideoDecodebin, NULL)){
        g_error("Failed to link pVideoRtpdepay->pVideoDecodebin \n");
    }
    
    g_signal_connect(pVideoDecodebin, "pad-added" , G_CALLBACK(add_pad) , pVideoConvert);

    if(!gst_element_link_many(pVideoConvert,pVideoSink,NULL)){
        g_error("Failed to link video pipeline \n");
        return -1;
    }

    /* audio pipe line */
    pAudioPipeline = gst_pipeline_new(NULL);
    g_assert(pAudioPipeline);

    pAudioRtspSrc = gst_element_factory_make("rtspsrc","audio_rtspsrc");
    g_assert(pAudioRtspSrc);
    if(rtsp_location[0] == '\0'){
        g_object_set(G_OBJECT(pAudioRtspSrc),"location", RTSP_LOCATION, "latency", 100, NULL);
        printf("Rtsp location: %s\n", RTSP_LOCATION);
    }
    else{
        g_object_set(G_OBJECT(pAudioRtspSrc),"location", rtsp_location, "latency", 100, NULL);
        printf("Rtsp location: %s\n", rtsp_location);
    }
    
    if(audio_rtp_depay[0] == '\0'){
        pAudioRtpdepay = gst_element_factory_make(AUDIO_RTP_DEPAY,"audio_rtp_depay");
        g_assert(pAudioRtpdepay);
    }
    else{
        pAudioRtpdepay = gst_element_factory_make(audio_rtp_depay,"audio_rtp_depay");
        g_assert(pAudioRtpdepay);
    }
       
    pAudioDecodebin = gst_element_factory_make("decodebin","audio_decodebin");
    g_assert(pAudioDecodebin);

    pAudioConvert = gst_element_factory_make("audioconvert","audio_convert");
    g_assert(pAudioConvert);

    pAudioSink = gst_element_factory_make("autoaudiosink","audio_sink");
    g_assert(pAudioSink);

    gst_bin_add_many(GST_BIN(pAudioPipeline),pAudioRtspSrc,pAudioRtpdepay,pAudioDecodebin,pAudioConvert,pAudioSink,NULL);

    g_signal_connect(pAudioRtspSrc, "pad-added", G_CALLBACK(cb_link_rtspsrc_pad), pAudioRtpdepay);

    if(!gst_element_link_many(pAudioRtpdepay, pAudioDecodebin, NULL)){
        g_error("Failed to link pAudioRtpdepay->pAudioDecodebin \n");
    }
    
    g_signal_connect(pAudioDecodebin, "pad-added" , G_CALLBACK(add_pad) , pAudioConvert);

    if(!gst_element_link_many(pAudioConvert,pAudioSink,NULL)){
        g_error("Failed to link Audio pipeline \n");
        return -1;
    }

    g_print("starting video pipeline\n");
    gst_element_set_state (pVideoPipeline, GST_STATE_PLAYING);
    g_print("starting audio pipeline\n");
    gst_element_set_state (pAudioPipeline, GST_STATE_PLAYING);
    loop=g_main_loop_new(NULL,FALSE);
    g_main_loop_run(loop);
    g_print("stopping video and audio pipeline\n");
    gst_element_set_state(pVideoPipeline,GST_STATE_NULL);
    gst_element_set_state(pAudioPipeline,GST_STATE_NULL);
#else
    MSC_RES res = MSC_OK;
    GMainLoop *loop = NULL;
    res = MSC_Init(&loop);
    if(MSC_OK != res){
        printf("MSC_Init() Fail....\n");
        return -1;
    }

    streamline_st videoStreamLine;
    if((rtsp_location[0] == '\0') && (video_rtp_depay[0] == '\0'))
        res = MSC_CreateVideoStreamLine(&videoStreamLine, NULL, NULL);
    else{
        if(rtsp_location[0] != '\0')
            res = MSC_CreateVideoStreamLine(&videoStreamLine, rtsp_location, NULL);
        else if(video_rtp_depay[0] != '\0'){
            res = MSC_CreateVideoStreamLine(&videoStreamLine, NULL, video_rtp_depay);
        }
        else{
            res = MSC_CreateVideoStreamLine(&videoStreamLine, rtsp_location, video_rtp_depay);
        }
    }
    if(MSC_OK != res){
        printf("MSC_CreateVideoStreamLine() Fail....\n");
        return -1;
    }


    streamline_st audioStreamLine;
    if((rtsp_location[0] == '\0') && (audio_rtp_depay[0] == '\0'))
        res = MSC_CreateAudioStreamLine(&audioStreamLine, NULL, NULL);
    else{
        if(rtsp_location[0] != '\0')
            res = MSC_CreateAudioStreamLine(&audioStreamLine, rtsp_location, NULL);
        else if(video_rtp_depay[0] != '\0'){
            res = MSC_CreateAudioStreamLine(&audioStreamLine, NULL, audio_rtp_depay);
        }
        else{
            res = MSC_CreateAudioStreamLine(&audioStreamLine, rtsp_location, audio_rtp_depay);
        }
    }
    
    if(MSC_OK != res){
        printf("MSC_CreateAudioStreamLine() Fail....\n");
        return -1;
    }

    /* 启动一个定时器控制video 的大小 */
    g_timeout_add_seconds (3, timeout_videosize_change_cb, &videoStreamLine);
    /* 启动一个定时器控制状态 */
    video_audio_pipeline_st video_audio_p;
    video_audio_p.pVideoPipeline = videoStreamLine.pPipeline;
    video_audio_p.pAudioPipeline = audioStreamLine.pPipeline;
    g_timeout_add_seconds (5, timeout_state_control_cb, &video_audio_p);
    MSC_PlayStart(loop, videoStreamLine.pPipeline, audioStreamLine.pPipeline);
#endif
    
    return 0;	
}

