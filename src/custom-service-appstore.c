#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dbus/dbus-glib.h>
#include "custom-service-appstore.h"
#include "custom-service-marshal.h"
#include "dbus-properties-client.h"
#include "dbus-shared.h"

/* DBus Prototypes */
static gboolean _custom_service_server_get_applications (CustomServiceAppstore * appstore, GArray ** apps);

#include "custom-service-server.h"

#define NOTIFICATION_ITEM_PROP_ID         "Id"
#define NOTIFICATION_ITEM_PROP_CATEGORY   "Category"
#define NOTIFICATION_ITEM_PROP_STATUS     "Status"
#define NOTIFICATION_ITEM_PROP_ICON_NAME  "IconName"
#define NOTIFICATION_ITEM_PROP_AICON_NAME "AttentionIconName"
#define NOTIFICATION_ITEM_PROP_MENU       "Menu"

/* Private Stuff */
typedef struct _CustomServiceAppstorePrivate CustomServiceAppstorePrivate;
struct _CustomServiceAppstorePrivate {
	DBusGConnection * bus;
	GList * applications;
};

typedef struct _Application Application;
struct _Application {
	gchar * dbus_name;
	gchar * dbus_object;
	CustomServiceAppstore * appstore; /* not ref'd */
	DBusGProxy * dbus_proxy;
	DBusGProxy * prop_proxy;
};

#define CUSTOM_SERVICE_APPSTORE_GET_PRIVATE(o) \
			(G_TYPE_INSTANCE_GET_PRIVATE ((o), CUSTOM_SERVICE_APPSTORE_TYPE, CustomServiceAppstorePrivate))

/* Signals Stuff */
enum {
	APPLICATION_ADDED,
	APPLICATION_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* GObject stuff */
static void custom_service_appstore_class_init (CustomServiceAppstoreClass *klass);
static void custom_service_appstore_init       (CustomServiceAppstore *self);
static void custom_service_appstore_dispose    (GObject *object);
static void custom_service_appstore_finalize   (GObject *object);

G_DEFINE_TYPE (CustomServiceAppstore, custom_service_appstore, G_TYPE_OBJECT);

static void
custom_service_appstore_class_init (CustomServiceAppstoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CustomServiceAppstorePrivate));

	object_class->dispose = custom_service_appstore_dispose;
	object_class->finalize = custom_service_appstore_finalize;

	signals[APPLICATION_ADDED] = g_signal_new ("application-added",
	                                           G_TYPE_FROM_CLASS(klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           G_STRUCT_OFFSET (CustomServiceAppstore, application_added),
	                                           NULL, NULL,
	                                           _custom_service_marshal_VOID__STRING_INT_STRING_STRING,
	                                           G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_NONE);
	signals[APPLICATION_REMOVED] = g_signal_new ("application-removed",
	                                           G_TYPE_FROM_CLASS(klass),
	                                           G_SIGNAL_RUN_LAST,
	                                           G_STRUCT_OFFSET (CustomServiceAppstore, application_removed),
	                                           NULL, NULL,
	                                           g_cclosure_marshal_VOID__INT,
	                                           G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_NONE);


	dbus_g_object_type_install_info(CUSTOM_SERVICE_APPSTORE_TYPE,
	                                &dbus_glib__custom_service_server_object_info);

	return;
}

static void
custom_service_appstore_init (CustomServiceAppstore *self)
{
	CustomServiceAppstorePrivate * priv = CUSTOM_SERVICE_APPSTORE_GET_PRIVATE(self);

	priv->applications = NULL;
	
	GError * error = NULL;
	priv->bus = dbus_g_bus_get(DBUS_BUS_STARTER, &error);
	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return;
	}

	dbus_g_connection_register_g_object(priv->bus,
	                                    INDICATOR_CUSTOM_DBUS_OBJ,
	                                    G_OBJECT(self));

	return;
}

static void
custom_service_appstore_dispose (GObject *object)
{
	CustomServiceAppstorePrivate * priv = CUSTOM_SERVICE_APPSTORE_GET_PRIVATE(object);

	while (priv->applications != NULL) {
		custom_service_appstore_application_remove(CUSTOM_SERVICE_APPSTORE(object),
		                                           ((Application *)priv->applications->data)->dbus_name,
		                                           ((Application *)priv->applications->data)->dbus_object);
	}

	G_OBJECT_CLASS (custom_service_appstore_parent_class)->dispose (object);
	return;
}

static void
custom_service_appstore_finalize (GObject *object)
{

	G_OBJECT_CLASS (custom_service_appstore_parent_class)->finalize (object);
	return;
}

static void
get_all_properties_cb (DBusGProxy * proxy, GHashTable * properties, GError * error, gpointer data)
{
	if (error != NULL) {
		g_warning("Unable to get properties: %s", error->message);
		return;
	}

	Application * app = (Application *)data;

	if (g_hash_table_lookup(properties, NOTIFICATION_ITEM_PROP_MENU) == NULL ||
			g_hash_table_lookup(properties, NOTIFICATION_ITEM_PROP_ICON_NAME) == NULL) {
		g_warning("Notification Item on object %s of %s doesn't have enough properties.", app->dbus_object, app->dbus_name);
		g_free(app); // Need to do more than this, but it gives the idea of the flow we're going for.
		return;
	}

	CustomServiceAppstorePrivate * priv = CUSTOM_SERVICE_APPSTORE_GET_PRIVATE(app->appstore);
	priv->applications = g_list_prepend(priv->applications, app);

	g_signal_emit(G_OBJECT(app->appstore),
	              signals[APPLICATION_ADDED], 0, 
	              g_value_get_string(g_hash_table_lookup(properties, NOTIFICATION_ITEM_PROP_ICON_NAME)),
	              0, /* Position */
	              app->dbus_name,
	              g_value_get_string(g_hash_table_lookup(properties, NOTIFICATION_ITEM_PROP_MENU)),
	              TRUE);

	return;
}

void
custom_service_appstore_application_add (CustomServiceAppstore * appstore, const gchar * dbus_name, const gchar * dbus_object)
{
	g_return_if_fail(IS_CUSTOM_SERVICE_APPSTORE(appstore));
	g_return_if_fail(dbus_name != NULL && dbus_name[0] != '\0');
	g_return_if_fail(dbus_object != NULL && dbus_object[0] != '\0');
	CustomServiceAppstorePrivate * priv = CUSTOM_SERVICE_APPSTORE_GET_PRIVATE(appstore);

	Application * app = g_new(Application, 1);

	app->dbus_name = g_strdup(dbus_name);
	app->dbus_object = g_strdup(dbus_object);
	app->appstore = appstore;

	GError * error = NULL;
	app->dbus_proxy = dbus_g_proxy_new_for_name_owner(priv->bus,
	                                                  app->dbus_name,
	                                                  app->dbus_object,
	                                                  NOTIFICATION_ITEM_DBUS_IFACE,
	                                                  &error);

	if (error != NULL) {
		g_warning("Unable to get notification item proxy for object '%s' on host '%s': %s", dbus_object, dbus_name, error->message);
		g_error_free(error);
		g_free(app);
		return;
	}

	app->prop_proxy = dbus_g_proxy_new_for_name_owner(priv->bus,
	                                                  app->dbus_name,
	                                                  app->dbus_object,
	                                                  DBUS_INTERFACE_PROPERTIES,
	                                                  &error);

	if (error != NULL) {
		g_warning("Unable to get property proxy for object '%s' on host '%s': %s", dbus_object, dbus_name, error->message);
		g_error_free(error);
		g_object_unref(app->dbus_proxy);
		g_free(app);
		return;
	}

	org_freedesktop_DBus_Properties_get_all_async(app->prop_proxy,
	                                              NOTIFICATION_ITEM_DBUS_IFACE,
	                                              get_all_properties_cb,
	                                              app);

	return;
}

void
custom_service_appstore_application_remove (CustomServiceAppstore * appstore, const gchar * dbus_name, const gchar * dbus_object)
{
	g_return_if_fail(IS_CUSTOM_SERVICE_APPSTORE(appstore));
	g_return_if_fail(dbus_name != NULL && dbus_name[0] != '\0');
	g_return_if_fail(dbus_object != NULL && dbus_object[0] != '\0');


	return;
}

/* DBus Interface */
static gboolean
_custom_service_server_get_applications (CustomServiceAppstore * appstore, GArray ** apps)
{

	return FALSE;
}

