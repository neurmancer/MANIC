#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int video_output_available(const char *path)
{
    struct stat info;

    if(lstat(path, &info) == 0 || errno != ENOENT) {
        fprintf(stderr, "Video output already exists or cannot be checked: %s\n", path);
        return(0);
    }
    return(1);
}

static int encode_video(const RenderConfig *config)
{
    size_t directory_length = strlen(config->frames_directory);
    size_t video_length = strlen(config->video_path);
    size_t input_size;
    char *input = NULL;
    char *temporary = NULL;
    char *output = NULL;
    char fps[32];
    char filter[160];
    const char *pixel_format;
    unsigned long long intervals = config->video_intervals;
    size_t position = 0;
    int fd = -1;
    int temporary_created = 0;
    pid_t pid;
    int status;
    int result = 1;

    if(directory_length > (SIZE_MAX - 32) / 2 || video_length > SIZE_MAX - 32) {return(1);}
    input_size = directory_length * 2 + 32;
    input = malloc(input_size);
    temporary = calloc(video_length + 16, 1);
    output = malloc(video_length + 22);
    if(input == NULL || temporary == NULL || output == NULL) {goto cleanup;}
    if(!video_output_available(config->video_path)) {goto cleanup;}
    memcpy(input, "file:", 5);
    position = 5;
    for(size_t i = 0; i < directory_length; i++) {
        if(config->frames_directory[i] == '%') {input[position++] = '%';}
        input[position++] = config->frames_directory[i];
    }
    snprintf(input + position, input_size - position, "/frame_%%06d.png");
    snprintf(temporary, video_length + 16, "%s.tmpXXXXXX", config->video_path);
    fd = mkstemp(temporary);
    if(fd < 0) {perror("video temporary file"); goto cleanup;}
    temporary_created = 1;
    if(close(fd) != 0) {fd = -1; perror("close"); goto cleanup;}
    fd = -1;
    snprintf(output, video_length + 22, "file:%s", temporary);
    snprintf(fps, sizeof(fps), "%d", config->fps);
    pixel_format = (config->width % 2 || config->height % 2) ? "yuv444p" : "yuv420p";
    snprintf(filter, sizeof(filter), "select='lt(n,%llu)+eq(n,%llu)',setpts=N/(%d*TB)",
        intervals > 0 ? intervals - 1 : 0, intervals, config->fps);
    pid = fork();
    if(pid < 0) {perror("fork"); goto cleanup;}
    if(pid == 0) {
        execlp("ffmpeg", "ffmpeg", "-nostdin", "-y", "-framerate", fps, "-start_number", "0",
            "-i", input, "-c:v", "libx264", "-pix_fmt", pixel_format,
            "-vf", filter, "-movflags", "+faststart", "-f", "mp4", output, (char *)NULL);
        perror("ffmpeg");
        _exit(127);
    }
    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR) {perror("waitpid"); goto cleanup;}
    }
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "FFmpeg failed; PNG frames were retained in %s\n", config->frames_directory);
        goto cleanup;
    }
    if(link(temporary, config->video_path) != 0) {perror("video output"); goto cleanup;}
    result = 0;

cleanup:
    if(fd >= 0) {close(fd);}
    if(temporary_created) {unlink(temporary);}
    free(input);
    free(temporary);
    free(output);
    return(result);
}
