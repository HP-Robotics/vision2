#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>


#include <gtkimageview/gtkimageview.h>

unsigned char *g_yuv_data = NULL;
int g_width = 0;
int g_height = 0;

static gint cb_mouse_wheel_scroll(GtkImageView *view,
                GdkScrollDirection direction,
                gpointer user_data)
{
    if (direction == GDK_SCROLL_UP)
        gtk_image_view_zoom_in(view);
    else
        gtk_image_view_zoom_out(view);
    return 0;
}

static gint cb_motion_event(GtkWidget *widget,
                GdkEventButton *event,
                gpointer user_data)
{
    gchar pixel_info[64];
    GtkWidget *label = GTK_WIDGET(user_data);
    GdkRectangle rect;
    unsigned char *yuv;
    unsigned char y;
    int img_x = 0, img_y = 0;
    double zoom = gtk_image_view_get_zoom(GTK_IMAGE_VIEW(widget));

    gtk_image_view_get_viewport(GTK_IMAGE_VIEW(widget), &rect);
    img_x = (int) floor((event->x + rect.x) / zoom);
    img_y = (int) floor((event->y + rect.y) / zoom);

    yuv = g_yuv_data + (img_y * g_width * 2) + ((img_x / 2) * 4);
    if (img_x % 2 == 0)
        y = *yuv;
    else
        y = *(yuv + 2);

    g_snprintf(pixel_info, sizeof(pixel_info), "(%d,%d) - [Y %d|U %d|V %d]", img_x, img_y,
            y, *(yuv + 1), *(yuv + 3));
    gtk_label_set_text(GTK_LABEL(label), pixel_info);


    return 0;
}

int main (int argc, char *argv[])
{
    GdkPixbuf *pixbuf;
    struct stat st;

    gtk_init (&argc, &argv);

    if (argc < 3)
    {
        printf("Need name of image, and name of data!\n");
        return -1;
    }
    else
    {
        GError *error = NULL;
        pixbuf = gdk_pixbuf_new_from_file (argv[1], &error);

        if (stat(argv[2], &st) != 0)
        {
            printf("Error finding %s\n", argv[2]);
            return -2;
        }
        g_yuv_data = malloc(st.st_size);
        int fd = open(argv[2], O_RDONLY);
        if (fd >= 0)
        {
            read(fd, g_yuv_data, st.st_size);
            close(fd);
        }
    }


    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_exit), NULL);

    GtkWidget *view = gtk_image_view_new ();
    gtk_image_view_set_pixbuf (GTK_IMAGE_VIEW (view), pixbuf, TRUE);

    g_width = gdk_pixbuf_get_width(pixbuf);
    g_height = gdk_pixbuf_get_height(pixbuf);

    gtk_widget_set_size_request (view, g_width < 640 ? g_width : 640, g_height < 480 ? g_height : 480);
    gtk_image_view_set_fitting(GTK_IMAGE_VIEW(view), FALSE);

    GtkWidget *vbox = gtk_vbox_new(0,0);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    GtkWidget *scrolled_win = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
         GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start (GTK_BOX(vbox), scrolled_win,
        TRUE, TRUE, 0);
    gtk_widget_show(scrolled_win);

    gtk_container_add (GTK_CONTAINER (scrolled_win), view);
    gtk_widget_show(view);

    GtkWidget *label = gtk_label_new("");
    gtk_box_pack_start (GTK_BOX(vbox),
                          label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    g_signal_connect(view, "mouse-wheel-scroll",
                    G_CALLBACK(cb_mouse_wheel_scroll), view);

    g_signal_connect (view, "motion_notify_event",
                    G_CALLBACK(cb_motion_event), label);

    gtk_widget_show_all (window);
    gtk_main ();

    return 0;
}
