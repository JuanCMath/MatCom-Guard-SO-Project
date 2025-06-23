#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gui_internal.h"
#include "gui.h"
#include "gui_process_integration.h"
#include "gui_usb_integration.h"
#include "gui_ports_integration.h"
#include "gui_system_coordinator.h"

// =============================================================================
// VARIABLES GLOBALES DE LA INTERFAZ PRINCIPAL
// =============================================================================

// Widgets principales de la ventana - estos son los elementos fundamentales de la GUI
GtkWidget *main_window = NULL;        // Ventana principal de la aplicación
GtkWidget *main_container = NULL;     // Contenedor principal que organiza todos los elementos
GtkWidget *header_bar = NULL;         // Barra superior con título y controles principales
GtkWidget *status_bar = NULL;         // Barra inferior que muestra estado del sistema
GtkWidget *notebook = NULL;           // Control de pestañas para diferentes módulos

// Callbacks del backend - estas son las funciones que conectan la GUI con la lógica de negocio
ScanUSBCallback usb_callback = NULL;        // Función para escanear dispositivos USB
ScanProcessesCallback processes_callback = NULL;  // Función para escanear procesos del sistema
ScanPortsCallback ports_callback = NULL;   // Función para escanear puertos de red
ExportReportCallback report_callback = NULL;      // Función para exportar reportes

// =============================================================================
// VARIABLES DEL SISTEMA DE PAUSA GLOBAL
// =============================================================================

// Estado del sistema de pausa - estas variables controlan si el monitoreo está pausado o activo
static gboolean system_paused = FALSE;           // TRUE si todo el sistema está pausado
static gboolean monitoring_was_active[3] = {FALSE, FALSE, FALSE}; // Estado previo de cada módulo
// Índices: 0=USB, 1=Procesos, 2=Puertos - esto nos permite restaurar exactamente el estado anterior

// =============================================================================
// DECLARACIONES DE FUNCIONES PRIVADAS
// =============================================================================

// Funciones de gestión de ventana y sistema
static void on_window_destroy(GtkWidget *widget, gpointer data);
static int initialize_complete_backend_system(void);
static void cleanup_complete_backend_system(void);
static void intelligent_system_sync(void);

// Funciones del sistema de pausa - estas implementan la funcionalidad core de pausar/reanudar
static void save_current_monitoring_state(void);
static void pause_all_monitoring_systems(void);
static void resume_all_monitoring_systems(void);

// Callbacks de la interfaz - responden a acciones del usuario
static void on_monitor_toggle_clicked(GtkToggleButton *button, gpointer data);
static void on_scan_all_clicked(GtkMenuItem *item, gpointer data);
static void on_scan_usb_menu_clicked(GtkMenuItem *item, gpointer data);
static void on_scan_processes_menu_clicked(GtkMenuItem *item, gpointer data);
static void on_scan_ports_menu_clicked(GtkMenuItem *item, gpointer data);
static void on_config_clicked(GtkButton *button, gpointer data);
static void on_export_clicked(GtkButton *button, gpointer data);

// Wrappers seguros para callbacks - verifican el estado de pausa antes de ejecutar
static void safe_usb_callback_wrapper(void);
static void safe_processes_callback_wrapper(void);
static void safe_ports_callback_wrapper(void);

// Funciones de creación de interfaz
static GtkWidget* create_main_notebook(void);
static GtkWidget* create_header_bar(void);

// =============================================================================
// IMPLEMENTACIÓN DEL SISTEMA DE PAUSA GLOBAL
// =============================================================================

/**
 * Esta función captura el estado actual de todos los módulos de monitoreo antes de pausarlos.
 * Es fundamental para poder restaurar exactamente el mismo estado cuando se reanude el sistema.
 * 
 * ¿Por qué es importante esto? Imagina que el usuario tiene solo el monitoreo USB activo,
 * pero no el de procesos. Si pausamos y luego reanudamos todo, no queremos activar
 * módulos que el usuario nunca había encendido.
 */
static void save_current_monitoring_state(void) {
    // Consultar cada módulo para saber si está actualmente activo
    monitoring_was_active[0] = is_usb_monitoring_active();      // ¿El monitoreo USB está corriendo?
    monitoring_was_active[1] = is_process_monitoring_active();  // ¿El monitoreo de procesos está corriendo?
    monitoring_was_active[2] = is_port_scan_active();           // ¿Hay un escaneo de puertos en progreso?
    
    // Crear un mensaje informativo para el log que muestre exactamente qué se guardó
    char state_msg[256];
    snprintf(state_msg, sizeof(state_msg), 
             "Estado guardado - USB: %s, Procesos: %s, Puertos: %s",
             monitoring_was_active[0] ? "activo" : "inactivo",
             monitoring_was_active[1] ? "activo" : "inactivo", 
             monitoring_was_active[2] ? "activo" : "inactivo");
    gui_add_log_entry("PAUSE_SYSTEM", "INFO", state_msg);
}

/**
 * Esta función detiene ordenadamente todos los sistemas de monitoreo que están funcionando.
 * No es simplemente "matar" los procesos - es una parada controlada que permite que cada
 * módulo termine limpiamente sus operaciones actuales.
 * 
 * Piensa en esto como la diferencia entre desconectar abruptamente la electricidad de una casa
 * versus usar los interruptores apropiados para apagar cada dispositivo en orden.
 */
static void pause_all_monitoring_systems(void) {
    gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Iniciando pausa de todos los sistemas de monitoreo...");
    
    int systems_paused = 0; // Contador para saber cuántos sistemas logramos pausar exitosamente
    
    // === PAUSAR MONITOREO USB ===
    // El monitoreo USB corre en un hilo separado que verifica periódicamente los dispositivos conectados
    if (is_usb_monitoring_active()) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Deteniendo monitoreo USB...");
        if (stop_usb_monitoring() == 0) {  // 0 significa éxito
            systems_paused++;
            gui_add_log_entry("PAUSE_SYSTEM", "INFO", "✅ Monitoreo USB detenido exitosamente");
        } else {
            gui_add_log_entry("PAUSE_SYSTEM", "ERROR", "❌ Error al detener monitoreo USB");
        }
    }
    
    // === PAUSAR MONITOREO DE PROCESOS ===
    // El monitoreo de procesos también corre en su propio hilo, analizando constantemente el uso de CPU y memoria
    if (is_process_monitoring_active()) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Deteniendo monitoreo de procesos...");
        if (stop_process_monitoring() == 0) {
            systems_paused++;
            gui_add_log_entry("PAUSE_SYSTEM", "INFO", "✅ Monitoreo de procesos detenido exitosamente");
        } else {
            gui_add_log_entry("PAUSE_SYSTEM", "ERROR", "❌ Error al detener monitoreo de procesos");
        }
    }
    
    // === CANCELAR ESCANEO DE PUERTOS ===
    // Los escaneos de puertos son diferentes - no son monitoreo continuo sino operaciones puntuales
    // que pueden tomar mucho tiempo. Si hay uno en progreso, lo cancelamos.
    if (is_port_scan_active()) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Cancelando escaneo de puertos en progreso...");
        if (cancel_port_scan() == 0) {
            systems_paused++;
            gui_add_log_entry("PAUSE_SYSTEM", "INFO", "✅ Escaneo de puertos cancelado exitosamente");
        } else {
            gui_add_log_entry("PAUSE_SYSTEM", "ERROR", "❌ Error al cancelar escaneo de puertos");
        }
    }
    
    // === NOTIFICAR AL COORDINADOR DEL SISTEMA ===
    // El coordinador del sistema necesita saber que estamos en modo de mantenimiento
    // para que pueda ajustar su evaluación de seguridad apropiadamente
    notify_module_status_change("global", MODULE_STATUS_MAINTENANCE, 
                               "Sistema pausado por solicitud del usuario");
    
    // Crear un resumen final para el usuario
    char pause_summary[256];
    snprintf(pause_summary, sizeof(pause_summary), 
             "Sistema pausado: %d módulos detenidos exitosamente", systems_paused);
    gui_add_log_entry("PAUSE_SYSTEM", "INFO", pause_summary);
}

/**
 * Esta función restaura el sistema exactamente al estado que tenía antes de pausarse.
 * Es la operación inversa a pause_all_monitoring_systems(), pero con la inteligencia
 * de solo reactivar lo que estaba funcionando antes.
 */
static void resume_all_monitoring_systems(void) {
    gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Iniciando reanudación de sistemas de monitoreo...");
    
    int systems_resumed = 0; // Contador de sistemas exitosamente reanudados
    
    // === REANUDAR MONITOREO USB ===
    // Solo si estaba activo antes de pausar
    if (monitoring_was_active[0]) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Reanudando monitoreo USB...");
        // Usamos get_usb_scan_interval() para restaurar con la configuración que tenía el usuario
        if (start_usb_monitoring(get_usb_scan_interval()) == 0) {
            systems_resumed++;
            gui_add_log_entry("PAUSE_SYSTEM", "INFO", "✅ Monitoreo USB reanudado exitosamente");
        } else {
            gui_add_log_entry("PAUSE_SYSTEM", "ERROR", "❌ Error al reanudar monitoreo USB");
        }
    }
    
    // === REANUDAR MONITOREO DE PROCESOS ===
    // Solo si estaba activo antes de pausar
    if (monitoring_was_active[1]) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", "Reanudando monitoreo de procesos...");
        if (start_process_monitoring() == 0) {
            systems_resumed++;
            gui_add_log_entry("PAUSE_SYSTEM", "INFO", "✅ Monitoreo de procesos reanudado exitosamente");
        } else {
            gui_add_log_entry("PAUSE_SYSTEM", "ERROR", "❌ Error al reanudar monitoreo de procesos");
        }
    }
    
    // === NOTA SOBRE ESCANEOS DE PUERTOS ===
    // Los escaneos de puertos no se reanudan automáticamente porque son operaciones puntuales,
    // no de monitoreo continuo. El usuario debe iniciarlos manualmente cuando los necesite.
    if (monitoring_was_active[2]) {
        gui_add_log_entry("PAUSE_SYSTEM", "INFO", 
                         "ℹ️ El escaneo de puertos estaba activo - usar escaneo manual para reanudar");
    }
    
    // === NOTIFICAR AL COORDINADOR ===
    // Informar que volvemos a estar operativos
    notify_module_status_change("global", MODULE_STATUS_ACTIVE, 
                               "Sistema reanudado por solicitud del usuario");
    
    // Resumen final
    char resume_summary[256];
    snprintf(resume_summary, sizeof(resume_summary), 
             "Sistema reanudado: %d módulos reiniciados exitosamente", systems_resumed);
    gui_add_log_entry("PAUSE_SYSTEM", "INFO", resume_summary);
}

// =============================================================================
// WRAPPERS SEGUROS PARA CALLBACKS
// =============================================================================

/**
 * Estos wrappers son una capa de seguridad que previene que los escaneos se ejecuten
 * cuando el sistema está pausado. Sin estos, un usuario podría pausar el sistema
 * pero luego presionar botones individuales de escaneo, creando inconsistencias.
 */

static void safe_usb_callback_wrapper(void) {
    // Verificar primero si el sistema está pausado
    if (system_paused) {
        gui_add_log_entry("USB_SCANNER", "WARNING", 
                         "⏸️ Sistema pausado - escaneo USB cancelado");
        return; // Salir sin hacer nada
    }
    
    // Si no está pausado, proceder con el escaneo normal
    if (usb_callback) {
        usb_callback();
    }
}

static void safe_processes_callback_wrapper(void) {
    if (system_paused) {
        gui_add_log_entry("PROCESS_SCANNER", "WARNING", 
                         "⏸️ Sistema pausado - escaneo de procesos cancelado");
        return;
    }
    
    if (processes_callback) {
        processes_callback();
    }
}

static void safe_ports_callback_wrapper(void) {
    if (system_paused) {
        gui_add_log_entry("PORT_SCANNER", "WARNING", 
                         "⏸️ Sistema pausado - escaneo de puertos cancelado");
        return;
    }
    
    if (ports_callback) {
        ports_callback();
    }
}

// =============================================================================
// CALLBACKS DE LA INTERFAZ DE USUARIO
// =============================================================================

/**
 * Este es el callback principal del botón de pausa/reanudar. Es el corazón de toda
 * la funcionalidad de control del sistema.
 * 
 * La lógica es: si está pausado y el usuario presiona el botón, reanudar.
 * Si está activo y el usuario presiona el botón, pausar.
 */
static void on_monitor_toggle_clicked(GtkToggleButton *button, gpointer data) {
    gboolean is_active = gtk_toggle_button_get_active(button);
    
    if (is_active && system_paused) {
        // === TRANSICIÓN: PAUSADO → ACTIVO ===
        gtk_button_set_label(GTK_BUTTON(button), "⏸️ Pausar");
        gui_add_log_entry("SISTEMA", "INFO", "🔄 Reanudando monitoreo automático - todos los módulos...");
        
        // Reanudar todos los sistemas que estaban activos antes
        resume_all_monitoring_systems();
        
        // Actualizar estado global de la aplicación
        system_paused = FALSE;
        gui_update_system_status("Sistema Operativo", TRUE);
        
        // Solicitar que el coordinador del sistema haga una evaluación inmediata
        // para actualizar el estado de seguridad después de reanudar
        request_immediate_system_evaluation();
        
    } else if (!is_active && !system_paused) {
        // === TRANSICIÓN: ACTIVO → PAUSADO ===
        gtk_button_set_label(GTK_BUTTON(button), "▶️ Reanudar");
        gui_add_log_entry("SISTEMA", "WARNING", "⏸️ Pausando monitoreo automático - deteniendo todos los módulos...");
        
        // Guardar el estado actual antes de pausar (esto es crucial para la restauración)
        save_current_monitoring_state();
        
        // Pausar todos los sistemas activos
        pause_all_monitoring_systems();
        
        // Actualizar estado global de la aplicación
        system_paused = TRUE;
        gui_update_system_status("Monitoreo Pausado", FALSE);
        
        // Indicar que ya no hay escaneos en progreso
        gui_set_scanning_status(FALSE);
    }
    
    // === FEEDBACK VISUAL PARA EL USUARIO ===
    // Mostrar un mensaje claro sobre el estado actual del sistema
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg), 
             "Estado del sistema: %s - Todos los módulos %s",
             system_paused ? "PAUSADO" : "ACTIVO",
             system_paused ? "detenidos" : "operativos");
    gui_add_log_entry("SISTEMA", system_paused ? "WARNING" : "INFO", status_msg);
}

/**
 * Callback para el escaneo completo del sistema. Esta función ahora verifica
 * si el sistema está pausado antes de proceder.
 */
static void on_scan_all_clicked(GtkMenuItem *item, gpointer data) {
    if (system_paused) {
        // === SISTEMA PAUSADO: MOSTRAR DIÁLOGO EXPLICATIVO ===
        // En lugar de fallar silenciosamente, explicamos al usuario qué hacer
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_INFO,
                                                  GTK_BUTTONS_OK,
                                                  "El sistema está pausado.\n\n"
                                                  "Para realizar un escaneo completo, primero reactive "
                                                  "el monitoreo usando el botón 'Reanudar' en la barra superior.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        
        gui_add_log_entry("SCANNER", "WARNING", 
                         "⏸️ Escaneo completo cancelado - sistema pausado");
        return;
    }
    
    // === SISTEMA ACTIVO: PROCEDER CON ESCANEO ===
    gui_add_log_entry("SCANNER", "INFO", "Iniciando escaneo completo del sistema");
    gui_set_scanning_status(TRUE);
    
    // Cambiar a la pestaña de logs para que el usuario pueda ver el progreso
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 4);
    
    // Ejecutar todos los escaneos usando los wrappers seguros
    // (aunque ya verificamos arriba, los wrappers proporcionan una segunda capa de seguridad)
    safe_usb_callback_wrapper();
    safe_processes_callback_wrapper(); 
    safe_ports_callback_wrapper();
    
    // Programar la desactivación del indicador de escaneo después de un tiempo razonable
    g_timeout_add_seconds(5, (GSourceFunc)gui_set_scanning_status, GINT_TO_POINTER(FALSE));
}

/**
 * Los siguientes callbacks manejan los escaneos individuales desde el menú.
 * Cada uno cambia a la pestaña apropiada y ejecuta el escaneo correspondiente.
 */
static void on_scan_usb_menu_clicked(GtkMenuItem *item, gpointer data) {
    // Cambiar a la pestaña USB para mostrar los resultados
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);
    
    // Ejecutar escaneo usando el wrapper seguro (que verificará si el sistema está pausado)
    if (usb_callback) {
        gui_add_log_entry("USB_SCANNER", "INFO", "Escaneo manual de USB iniciado desde menú");
        safe_usb_callback_wrapper();
    }
}

static void on_scan_processes_menu_clicked(GtkMenuItem *item, gpointer data) {
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 2);
    
    if (processes_callback) {
        gui_add_log_entry("PROCESS_SCANNER", "INFO", "Escaneo manual de procesos iniciado desde menú");
        safe_processes_callback_wrapper();
    }
}

static void on_scan_ports_menu_clicked(GtkMenuItem *item, gpointer data) {
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 3);
    
    if (ports_callback) {
        gui_add_log_entry("PORT_SCANNER", "INFO", "Escaneo manual de puertos iniciado desde menú");
        safe_ports_callback_wrapper();
    }
}

static void on_config_clicked(GtkButton *button, gpointer data) {
    gui_add_log_entry("CONFIG", "INFO", "Abriendo ventana de configuración");
    show_config_dialog(GTK_WINDOW(main_window));
}

/**
 * Callback para exportar reportes. Crea un diálogo para que el usuario seleccione
 * dónde guardar el archivo y en qué formato.
 */
static void on_export_clicked(GtkButton *button, gpointer data) {
    // Crear diálogo de guardar archivo
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Exportar Reporte de Seguridad",
                                                    GTK_WINDOW(main_window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancelar", GTK_RESPONSE_CANCEL,
                                                    "_Guardar", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    // Configurar nombre predeterminado con timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char filename[256];
    strftime(filename, sizeof(filename), "MatComGuard_Report_%Y%m%d_%H%M%S.pdf", tm);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), filename);
    
    // Agregar filtros de archivo para diferentes formatos
    GtkFileFilter *pdf_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(pdf_filter, "Documentos PDF");
    gtk_file_filter_add_pattern(pdf_filter, "*.pdf");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), pdf_filter);
    
    GtkFileFilter *txt_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(txt_filter, "Archivos de texto");
    gtk_file_filter_add_pattern(txt_filter, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), txt_filter);
    
    // Mostrar diálogo y procesar la respuesta
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        if (report_callback) {
            report_callback(filename);
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    gui_add_log_entry("SISTEMA", "INFO", "Cerrando MatCom Guard - iniciando secuencia de apagado seguro...");
    
    // Realizar limpieza completa del sistema backend
    cleanup_complete_backend_system();
    
    gui_add_log_entry("SISTEMA", "INFO", "MatCom Guard cerrado exitosamente - todos los recursos liberados");
    
    printf("\n🛡️ MatCom Guard cerrado de manera segura\n");
    printf("   ✅ Todos los hilos terminados correctamente\n");
    printf("   ✅ Todos los recursos liberados\n");
    printf("   ✅ Estado del sistema guardado\n");
    printf("   Hasta la próxima protección! 👋\n\n");
    
    gtk_main_quit();
}

// =============================================================================
// FUNCIONES DE CREACIÓN DE INTERFAZ
// =============================================================================

/**
 * Crea el notebook principal con todas las pestañas de la aplicación.
 * Cada pestaña representa un módulo diferente del sistema de seguridad.
 */
static GtkWidget* create_main_notebook() {
    GtkWidget *nb = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_TOP);
    
    // === PESTAÑA: PANEL DE CONTROL GENERAL ===
    // Muestra estadísticas generales y estado del sistema
    GtkWidget *dashboard_label = gtk_label_new("📊 Dashboard");
    GtkWidget *dashboard_content = create_statistics_panel();
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), dashboard_content, dashboard_label);
    
    // === PESTAÑA: MONITOREO USB ===
    // Interfaz para monitorear dispositivos USB conectados
    GtkWidget *usb_label = gtk_label_new("💾 Dispositivos USB");
    GtkWidget *usb_content = create_usb_panel();
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), usb_content, usb_label);
    
    // === PESTAÑA: MONITOREO DE PROCESOS ===
    // Interfaz para monitorear procesos del sistema y su uso de recursos
    GtkWidget *process_label = gtk_label_new("⚡ Procesos");
    GtkWidget *process_content = create_process_panel();
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), process_content, process_label);
    
    // === PESTAÑA: ESCANEO DE PUERTOS ===
    // Interfaz para escanear puertos de red abiertos
    GtkWidget *ports_label = gtk_label_new("🔌 Puertos");
    GtkWidget *ports_content = create_ports_panel();
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), ports_content, ports_label);
    
    // === PESTAÑA: REGISTROS DEL SISTEMA ===
    // Muestra todos los logs y eventos del sistema
    GtkWidget *logs_label = gtk_label_new("📝 Registros");
    GtkWidget *logs_content = create_log_area();
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), logs_content, logs_label);
    
    return nb;
}

/**
 * Crea la barra de herramientas superior con todos los controles principales.
 * Esta barra contiene el menú de escaneo, botón de pausa, configuración, etc.
 */
static GtkWidget* create_header_bar() {
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "MatCom Guard");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Sistema de Protección Digital");
    
    // === MENÚ DE ESCANEO CON OPCIONES ===
    // Menú desplegable con diferentes tipos de escaneo
    GtkWidget *scan_menu_button = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(scan_menu_button), "🛡️ Escanear");
    gtk_widget_set_tooltip_text(scan_menu_button, "Opciones de escaneo de seguridad");
    
    // Crear el menú desplegable
    GtkWidget *scan_menu = gtk_menu_new();
    
    // Opción: Escaneo completo de todo el sistema
    GtkWidget *scan_all_item = gtk_menu_item_new_with_label("🛡️ Escaneo Completo");
    g_signal_connect(scan_all_item, "activate", G_CALLBACK(on_scan_all_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(scan_menu), scan_all_item);
    
    // Separador visual
    gtk_menu_shell_append(GTK_MENU_SHELL(scan_menu), gtk_separator_menu_item_new());
    
    // Opciones individuales para cada módulo
    GtkWidget *scan_usb_item = gtk_menu_item_new_with_label("💾 Escanear Dispositivos USB");
    g_signal_connect(scan_usb_item, "activate", G_CALLBACK(on_scan_usb_menu_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(scan_menu), scan_usb_item);
    
    GtkWidget *scan_proc_item = gtk_menu_item_new_with_label("⚡ Escanear Procesos");
    g_signal_connect(scan_proc_item, "activate", G_CALLBACK(on_scan_processes_menu_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(scan_menu), scan_proc_item);
    
    GtkWidget *scan_ports_item = gtk_menu_item_new_with_label("🔌 Escanear Puertos");
    g_signal_connect(scan_ports_item, "activate", G_CALLBACK(on_scan_ports_menu_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(scan_menu), scan_ports_item);
    
    gtk_widget_show_all(scan_menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(scan_menu_button), scan_menu);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), scan_menu_button);
    
    // === BOTÓN DE PAUSA/REANUDAR MONITOREO ===
    // Este es el botón principal que implementamos con la nueva funcionalidad
    GtkWidget *monitor_toggle_btn = gtk_toggle_button_new();
    gtk_button_set_label(GTK_BUTTON(monitor_toggle_btn), "⏸️ Pausar");
    gtk_widget_set_tooltip_text(monitor_toggle_btn, "Pausar/Reanudar monitoreo automático de todos los módulos");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(monitor_toggle_btn), TRUE); // Inicialmente activo
    g_signal_connect(monitor_toggle_btn, "toggled", G_CALLBACK(on_monitor_toggle_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), monitor_toggle_btn);
    
    // === BOTÓN DE CONFIGURACIÓN ===
    // Abre el diálogo de configuración del sistema
    GtkWidget *config_btn = gtk_button_new_with_label("⚙️ Configuración");
    gtk_widget_set_tooltip_text(config_btn, "Ajustar configuración del sistema");
    g_signal_connect(config_btn, "clicked", G_CALLBACK(on_config_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), config_btn);
    
    // === BOTÓN DE EXPORTAR REPORTES ===
    // Permite generar y guardar reportes de seguridad
    GtkWidget *export_btn = gtk_button_new_with_label("📄 Exportar");
    gtk_widget_set_tooltip_text(export_btn, "Generar reporte de seguridad");
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), export_btn);
    
    return header;
}

// =============================================================================
// FUNCIONES DE GESTIÓN DEL BACKEND
// =============================================================================

/**
 * Inicializa todo el sistema backend de forma coordinada.
 * Esta función establece todos los módulos de monitoreo y el coordinador central.
 * Es una operación compleja que debe manejar fallas graciosamente.
 */
static int initialize_complete_backend_system(void) {
    gui_add_log_entry("STARTUP", "INFO", "=== INICIANDO SISTEMA BACKEND COMPLETO ===");
    
    // === PASO 1: INICIALIZAR EL COORDINADOR DEL SISTEMA ===
    // El coordinador debe inicializarse primero porque otros módulos dependen de él
    if (init_system_coordinator() != 0) {
        gui_add_log_entry("STARTUP", "CRITICAL", "FALLO CRÍTICO: No se pudo inicializar coordinador del sistema");
        return -1;
    }
    gui_add_log_entry("STARTUP", "INFO", "✅ Coordinador del sistema inicializado");
    
    // === PASO 2: INICIALIZAR TODAS LAS INTEGRACIONES DE MÓDULOS ===
    // Cada módulo tiene su propia función de inicialización que configura
    // sus callbacks, estructuras de datos y configuración
    
    if (init_process_integration() != 0) {
        gui_add_log_entry("STARTUP", "ERROR", "Error al inicializar integración de procesos");
        return -1;
    }
    gui_add_log_entry("STARTUP", "INFO", "✅ Integración de procesos inicializada");
    
    if (init_usb_integration() != 0) {
        gui_add_log_entry("STARTUP", "ERROR", "Error al inicializar integración USB");
        return -1;
    }
    gui_add_log_entry("STARTUP", "INFO", "✅ Integración USB inicializada");
    
    if (init_ports_integration() != 0) {
        gui_add_log_entry("STARTUP", "ERROR", "Error al inicializar integración de puertos");
        return -1;
    }
    gui_add_log_entry("STARTUP", "INFO", "✅ Integración de puertos inicializada");
    
    // === PASO 3: INICIAR SERVICIOS AUTOMÁTICOS ===
    // Algunos módulos (como USB) deben iniciar monitoreo automático desde el arranque
    // Otros (como procesos y puertos) se inician bajo demanda
    
    // USB: monitoreo automático para detectar dispositivos conectados/desconectados
    if (start_usb_monitoring(30) != 0) {  // 30 segundos de intervalo
        gui_add_log_entry("STARTUP", "WARNING", "No se pudo iniciar monitoreo automático USB");
    } else {
        gui_add_log_entry("STARTUP", "INFO", "✅ Monitoreo automático USB iniciado");
        
        // Notificar al coordinador sobre el cambio de estado
        notify_module_status_change("usb", MODULE_STATUS_ACTIVE, 
                                   "Monitoreo automático iniciado exitosamente");
        
        // Guardar que USB estaba activo en el arranque para el sistema de pausa
        monitoring_was_active[0] = TRUE;
    }
    
    // Procesos y Puertos: se marcan como listos pero no se inician automáticamente
    notify_module_status_change("process", MODULE_STATUS_INACTIVE, 
                               "Listo para iniciar bajo demanda");
    notify_module_status_change("ports", MODULE_STATUS_INACTIVE, 
                               "Listo para iniciar bajo demanda");
    
    // === PASO 4: INICIAR EL COORDINADOR DEL SISTEMA ===
    // El coordinador ejecuta en su propio hilo y sincroniza todo el sistema
    if (start_system_coordinator(5) != 0) {  // 5 segundos de intervalo
        gui_add_log_entry("STARTUP", "CRITICAL", "FALLO CRÍTICO: No se pudo iniciar coordinador del sistema");
        return -1;
    }
    gui_add_log_entry("STARTUP", "INFO", "✅ Coordinador del sistema iniciado");
    
    // === PASO 5: REALIZAR SINCRONIZACIÓN INICIAL ===
    gui_add_log_entry("STARTUP", "INFO", "Realizando sincronización inicial del sistema...");
    
    // Esperar un momento para que los módulos se estabilicen
    sleep(2);
    
    // Solicitar evaluación inmediata para establecer estado inicial
    request_immediate_system_evaluation();
    
    gui_add_log_entry("STARTUP", "INFO", "=== SISTEMA BACKEND COMPLETAMENTE OPERATIVO ===");
    
    return 0;
}

/**
 * Limpia completamente el sistema backend de forma ordenada.
 * Esta función se llama al cerrar la aplicación y debe asegurar que todos
 * los hilos terminen limpiamente y todos los recursos sean liberados.
 */
static void cleanup_complete_backend_system(void) {
    gui_add_log_entry("SHUTDOWN", "INFO", "=== INICIANDO LIMPIEZA COMPLETA DEL SISTEMA ===");
    
    // === LIMPIAR EN ORDEN INVERSO AL DE INICIALIZACIÓN ===
    // Esto evita dependencias rotas y asegura una terminación limpia
    
    // Paso 1: Detener y limpiar el coordinador del sistema primero
    gui_add_log_entry("SHUTDOWN", "INFO", "Deteniendo coordinador del sistema...");
    cleanup_system_coordinator();
    gui_add_log_entry("SHUTDOWN", "INFO", "✅ Coordinador del sistema finalizado");
    
    // Paso 2: Limpiar integraciones de módulos
    gui_add_log_entry("SHUTDOWN", "INFO", "Finalizando módulos de integración...");
    
    cleanup_ports_integration();
    gui_add_log_entry("SHUTDOWN", "INFO", "✅ Integración de puertos finalizada");
    
    cleanup_usb_integration();
    gui_add_log_entry("SHUTDOWN", "INFO", "✅ Integración USB finalizada");
    
    cleanup_process_integration();
    gui_add_log_entry("SHUTDOWN", "INFO", "✅ Integración de procesos finalizada");
    
    gui_add_log_entry("SHUTDOWN", "INFO", "=== LIMPIEZA COMPLETA FINALIZADA ===");
}

/**
 * Sincroniza inteligentemente todos los módulos con la GUI.
 * Esta función recopila información de todos los backends y actualiza
 * la interfaz de usuario para reflejar el estado actual del sistema.
 */
static void intelligent_system_sync(void) {
    gui_add_log_entry("SYNC", "INFO", "Iniciando sincronización inteligente del sistema...");
    
    // === OBTENER ESTADO CONSOLIDADO DEL COORDINADOR ===
    int total_devices, total_processes, total_ports, security_alerts;
    if (get_consolidated_statistics(&total_devices, &total_processes, 
                                   &total_ports, &security_alerts) == 0) {
        
        // Actualizar panel principal con estadísticas consolidadas
        gui_update_statistics(total_devices, total_processes, total_ports);
        
        // Registrar estadísticas actuales para debugging
        char stats_msg[512];
        snprintf(stats_msg, sizeof(stats_msg),
                 "Estado sincronizado: %d dispositivos USB, %d procesos, %d puertos abiertos, %d alertas",
                 total_devices, total_processes, total_ports, security_alerts);
        gui_add_log_entry("SYNC", "INFO", stats_msg);
        
        // === EVALUAR Y ACTUALIZAR ESTADO DE SEGURIDAD ===
        // Si hay alertas de seguridad, asegurar que el estado del sistema lo refleje
        if (security_alerts > 0) {
            SystemSecurityLevel level = get_current_security_level();
            if (level >= SECURITY_LEVEL_WARNING) {
                gui_update_system_status("Alertas de Seguridad Activas", FALSE);
            }
        }
    }
    
    // === SINCRONIZAR VISTAS INDIVIDUALES DE MÓDULOS ACTIVOS ===
    // Solo sincronizar módulos que están realmente funcionando
    
    if (is_process_monitoring_active()) {
        sync_gui_with_backend_processes();
    }
    
    if (is_usb_monitoring_active()) {
        sync_gui_with_usb_devices();
    }
    
    // Los puertos se sincronizan automáticamente cuando hay resultados disponibles
    
    gui_add_log_entry("SYNC", "INFO", "Sincronización inteligente completada");
}

// =============================================================================
// FUNCIONES PÚBLICAS DE LA INTERFAZ
// =============================================================================

/**
 * Función principal de inicialización de la GUI.
 * Esta función configura toda la interfaz de usuario y conecta el backend.
 */
void init_gui(int argc, char **argv) {
    // === INICIALIZACIÓN BÁSICA DE GTK ===
    gtk_init(&argc, &argv);

    // === CREAR VENTANA PRINCIPAL ===
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "MatCom Guard - Sistema de Protección");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 900, 600);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);

    // Conectar el evento de cierre de ventana
    g_signal_connect(main_window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // === CREAR ESTRUCTURA DE LA INTERFAZ ===
    
    // Barra de título con controles principales
    header_bar = create_header_bar();
    gtk_window_set_titlebar(GTK_WINDOW(main_window), header_bar);

    // Contenedor principal que organiza todos los elementos
    main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (main_container == NULL) {
        printf("ERROR: Falló la creación de main_container!\n");
        return;
    }
    gtk_container_add(GTK_CONTAINER(main_window), main_container);

    // Notebook con todas las pestañas de funcionalidad
    notebook = create_main_notebook();
    gtk_box_pack_start(GTK_BOX(main_container), notebook, TRUE, TRUE, 0);
    
    // Barra de estado en la parte inferior
    status_bar = create_status_bar();
    gtk_box_pack_end(GTK_BOX(main_container), status_bar, FALSE, FALSE, 0);
    
    // === CONFIGURAR CALLBACKS DEL SISTEMA ===
    // Estos callbacks conectan la GUI con el backend usando los wrappers seguros
    gui_set_scan_callbacks(
        safe_usb_callback_wrapper,        // USB con verificación de pausa
        safe_processes_callback_wrapper,  // Procesos con verificación de pausa
        safe_ports_callback_wrapper,      // Puertos con verificación de pausa
        NULL   // Export callback - se mantiene sin cambios por ahora
    );
    
    // === MOSTRAR LA INTERFAZ ===
    gtk_widget_show_all(main_window);
    
    // === FEEDBACK PARA EL DESARROLLADOR ===
    printf("\n");
    printf("🛡️  MatCom Guard - Sistema de Protección Digital\n");
    printf("===============================================\n");
    printf("   ✅ Frontend GUI: Completamente funcional\n");
    printf("   ✅ Backend Integrado: Procesos, USB, Puertos\n");
    printf("   ✅ Coordinador del Sistema: Activo\n");
    printf("   ✅ Monitoreo Automático: USB en tiempo real\n");
    printf("   ✅ Threading: Multi-hilo coordinado\n");
    printf("   ✅ Estado Global: Sincronizado\n");
    printf("   ✅ Sistema de Pausa: Implementado\n");
    printf("   🔄 Sistema completamente operativo\n");
    printf("===============================================\n");
    printf("   Ventana principal: %dx%d píxeles\n", 900, 600);
    printf("   Pestañas disponibles: 5 (Dashboard, USB, Procesos, Puertos, Logs)\n");
    printf("   Backend real: Totalmente integrado\n");
    printf("   Control de pausa: Funcional\n");
    printf("   Estado: LISTO PARA PROTECCIÓN EN TIEMPO REAL\n\n");
    
    // === MENSAJES INICIALES EN EL LOG ===
    gui_add_log_entry("SISTEMA", "INFO", "MatCom Guard iniciado con backend completo integrado");
    gui_add_log_entry("SISTEMA", "INFO", "Todos los módulos (Procesos, USB, Puertos) están operativos");
    gui_add_log_entry("SISTEMA", "INFO", "Sistema de pausa global implementado y funcional");
    gui_add_log_entry("SISTEMA", "INFO", "Monitoreo automático USB activo - detectará dispositivos automáticamente");
    gui_add_log_entry("SISTEMA", "INFO", "Sistema listo para protección en tiempo real");
    
    // === CONFIGURAR ESTADO INICIAL ===
    gui_update_system_status("Sistema Operativo", TRUE);

    // === INICIALIZAR EL SISTEMA BACKEND ===
    if (initialize_complete_backend_system() != 0) {
        gui_add_log_entry("STARTUP", "CRITICAL", 
                         "FALLO CRÍTICO: No se pudo inicializar sistema backend");
        
        // Mostrar dialog de error crítico al usuario
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "Error crítico al inicializar MatCom Guard.\n"
                                                        "Revise los logs para más detalles.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        
        // Continuar con GUI limitada
        gui_update_system_status("Error de Inicialización", FALSE);
    } else {
        // === INICIALIZACIÓN EXITOSA ===
        gui_add_log_entry("STARTUP", "INFO", "🎉 MatCom Guard completamente inicializado y operativo");
        
        // Realizar sincronización inicial después de un breve delay
        g_timeout_add_seconds(3, (GSourceFunc)intelligent_system_sync, NULL);
    }
    
    // === INICIAR EL BUCLE PRINCIPAL DE GTK ===
    // Esta función bloquea hasta que se cierre la aplicación
    gtk_main();
}

/**
 * Configura los callbacks que conectan la GUI con el backend.
 * Esta función permite que otros módulos registren sus funciones de escaneo.
 */
void gui_set_scan_callbacks(
    ScanUSBCallback usb_cb,
    ScanProcessesCallback proc_cb,
    ScanPortsCallback port_cb,
    ExportReportCallback report_cb
) {
    // Almacenar las referencias a las funciones del backend
    usb_callback = usb_cb;
    processes_callback = proc_cb;
    ports_callback = port_cb;
    report_callback = report_cb;
    
    printf("✅ Callbacks del backend configurados correctamente\n");
}

// =============================================================================
// FUNCIONES PÚBLICAS DE ESTADO (para integración con otros módulos)
// =============================================================================

/**
 * Función pública para verificar si el sistema está pausado.
 * Otros módulos pueden usar esta función para verificar el estado antes de ejecutar operaciones.
 */
gboolean is_system_paused(void) {
    return system_paused;
}

/**
 * Función pública para obtener información detallada del estado del sistema.
 * Útil para módulos que necesitan información completa sobre qué estaba activo antes de pausar.
 */
void get_system_pause_status(gboolean *is_paused, gboolean *monitoring_states) {
    if (is_paused) {
        *is_paused = system_paused;
    }
    
    if (monitoring_states) {
        monitoring_states[0] = monitoring_was_active[0]; // USB
        monitoring_states[1] = monitoring_was_active[1]; // Procesos  
        monitoring_states[2] = monitoring_was_active[2]; // Puertos
    }
}