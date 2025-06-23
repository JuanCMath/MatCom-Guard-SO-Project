#include "gui_internal.h"
#include "gui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Variables del panel de procesos
static GtkWidget *process_tree_view = NULL;
static GtkListStore *process_list_store = NULL;
static GtkWidget *process_info_label = NULL;
static GtkWidget *scan_process_button = NULL;
static GtkWidget *kill_process_button = NULL;
static GtkWidget *process_panel_container = NULL;
static GtkWidget *cpu_threshold_spin = NULL;
static GtkWidget *mem_threshold_spin = NULL;

// Declaraciones de funciones auxiliares
static void format_cpu_column(GtkTreeViewColumn *column,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data);
static void format_mem_column(GtkTreeViewColumn *column,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data);
                             
// Columnas del TreeView
enum {
    COL_PROC_ICON,
    COL_PROC_PID,
    COL_PROC_NAME,
    COL_PROC_CPU_USAGE,
    COL_PROC_MEM_USAGE,
    COL_PROC_STATUS,
    COL_PROC_STATUS_COLOR,
    COL_PROC_CPU_COLOR,
    COL_PROC_MEM_COLOR,
    NUM_PROC_COLS
};

// Función para determinar el color según el porcentaje de uso
static const char* get_usage_color(float usage, float threshold) {
    if (usage > threshold) {
        return "#F44336"; // Rojo - Alto
    } else if (usage > threshold * 0.7) {
        return "#FF9800"; // Naranja - Medio-alto
    } else if (usage > threshold * 0.5) {
        return "#FFC107"; // Amarillo - Medio
    } else {
        return "#4CAF50"; // Verde - Bajo
    }
}

// Función para determinar el icono según el estado del proceso
static const char* get_process_icon(float cpu_usage, float mem_usage, gboolean is_suspicious) {
    if (is_suspicious) {
        return "🚨"; // Alerta máxima
    } else if (cpu_usage > 80.0 || mem_usage > 80.0) {
        return "⚠️"; // Advertencia
    } else if (cpu_usage > 50.0 || mem_usage > 50.0) {
        return "⚡"; // Alto consumo
    } else {
        return "✓"; // Normal
    }
}

// Callback para cuando se selecciona un proceso
static void on_process_selection_changed(GtkTreeSelection *selection, gpointer data) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint pid;
        gchar *name, *status;
        gfloat cpu_usage, mem_usage;
        
        gtk_tree_model_get(model, &iter,
                          COL_PROC_PID, &pid,
                          COL_PROC_NAME, &name,
                          COL_PROC_CPU_USAGE, &cpu_usage,
                          COL_PROC_MEM_USAGE, &mem_usage,
                          COL_PROC_STATUS, &status,
                          -1);
        
        // Actualizar el label de información
        char info_text[512];
        snprintf(info_text, sizeof(info_text),
                "<b>Proceso:</b> %s\n"
                "<b>PID:</b> %d\n"
                "<b>CPU:</b> %.1f%%\n"
                "<b>Memoria:</b> %.1f%%\n"
                "<b>Estado:</b> %s\n\n"
                "<i>Seleccionado para monitoreo detallado</i>",
                name, pid, cpu_usage, mem_usage, status);
        
        gtk_label_set_markup(GTK_LABEL(process_info_label), info_text);
        
        // Habilitar el botón de terminar proceso
        gtk_widget_set_sensitive(kill_process_button, TRUE);
        
        g_free(name);
        g_free(status);
    } else {
        gtk_widget_set_sensitive(kill_process_button, FALSE);
    }
}

// Callback para re-habilitar botón después del escaneo
static gboolean re_enable_process_button(gpointer data) {
    GtkButton *button = GTK_BUTTON(data);
    
    printf(">>> Timer ejecutándose - verificando estado del escaneo\n");
    
    if (!is_gui_process_scan_in_progress()) {
        printf(">>> Escaneo completado - rehabilitando botón\n");
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        gtk_button_set_label(button, "🔍 Escanear Procesos");
        gui_set_scanning_status(FALSE);
        printf(">>> Botón rehabilitado exitosamente\n");
        return G_SOURCE_REMOVE; // Detener el timer
    } else {
        printf(">>> Escaneo aún en progreso - continuando timer\n");
        return G_SOURCE_CONTINUE; // Continuar verificando
    }
}

// Callback para el botón de escaneo de procesos
static void on_scan_processes_clicked(GtkButton *button, gpointer data) {
    printf("=== DIAGNÓSTICO PROCESO ESCANEO ===\n");
    printf("1. Botón presionado\n");
    
    if (!processes_callback) {
        printf("ERROR: processes_callback es NULL\n");
        gui_add_log_entry("PROCESS_SCANNER", "WARNING", "No hay callback de escaneo de procesos configurado");
        return;
    }
    printf("2. Callback verificado - OK\n");
    
    if (is_gui_process_scan_in_progress()) {
        printf("WARNING: Escaneo ya en progreso\n");
        gui_add_log_entry("PROCESS_SCANNER", "WARNING", "Escaneo de procesos ya en progreso");
        return;
    }
    printf("3. Estado verificado - no hay escaneo previo\n");
    
    gui_add_log_entry("PROCESS_SCANNER", "INFO", "Iniciando escaneo de procesos del sistema");
    gui_set_scanning_status(TRUE);
    
    // Deshabilitar el botón durante el escaneo
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_button_set_label(button, "🔄 Escaneando...");
    printf("4. Botón deshabilitado y etiqueta cambiada\n");
    
    // Iniciar escaneo en hilo separado
    printf("5. Llamando a gui_compatible_scan_processes()\n");
    gui_compatible_scan_processes();
    printf("6. gui_compatible_scan_processes() retornó\n");
    
    // Verificar periódicamente si terminó el escaneo
    printf("7. Configurando timer de verificación\n");
    g_timeout_add_seconds(1, re_enable_process_button, button);
    printf("=== FIN DIAGNÓSTICO - Timer configurado ===\n");
}

// Estructura para manejar operaciones de terminación asíncronas
typedef struct {
    pid_t target_pid;           // PID del proceso a terminar
    char process_name[256];     // Nombre del proceso para logging
    int attempts_made;          // Número de intentos realizados
    time_t start_time;          // Cuándo comenzó el intento de terminación
    GtkTreeIter tree_iter;      // Iterador para remover de la GUI si es exitoso
    gboolean iter_valid;        // Si el iterador sigue siendo válido
} ProcessTerminationContext;

// Función auxiliar para verificar si un proceso sigue vivo
static gboolean is_process_still_alive(pid_t pid) {
    // Enviar señal 0 (no hace nada) solo para verificar si el proceso existe
    // Si kill() retorna 0, el proceso existe. Si retorna -1 con errno=ESRCH, no existe.
    int result = kill(pid, 0);
    
    if (result == 0) {
        return TRUE;  // El proceso sigue vivo
    } else if (errno == ESRCH) {
        return FALSE; // El proceso ya no existe (No such process)
    } else {
        // Otros errores (como EPERM - sin permisos) indican que el proceso existe
        // pero no tenemos permisos para afectarlo
        return TRUE;
    }
}

// Función para realizar un intento escalado de terminación
static gint attempt_process_termination(pid_t pid, const char* process_name, int attempt_number) {
    int signal_to_send;
    const char* signal_name;
    
    /*
     * ESTRATEGIA DE TERMINACIÓN ESCALADA:
     * 
     * Intento 1: SIGTERM (15) - Terminación amigable
     *   - Le dice al proceso "por favor termina limpiamente"
     *   - El proceso puede limpiar recursos, guardar datos, etc.
     *   - Algunos procesos pueden ignorar esta señal
     * 
     * Intento 2: SIGINT (2) - Interrupción (Ctrl+C)
     *   - Similar a cuando presionas Ctrl+C en la terminal
     *   - Algunos procesos responden mejor a esta señal
     * 
     * Intento 3: SIGKILL (9) - Terminación forzada
     *   - El kernel mata el proceso inmediatamente
     *   - No puede ser ignorada ni bloqueada
     *   - Último recurso porque no permite limpieza
     */
    
    switch (attempt_number) {
        case 1:
            signal_to_send = SIGTERM;
            signal_name = "SIGTERM (terminación amigable)";
            break;
        case 2:
            signal_to_send = SIGINT;
            signal_name = "SIGINT (interrupción)";
            break;
        case 3:
            signal_to_send = SIGKILL;
            signal_name = "SIGKILL (terminación forzada)";
            break;
        default:
            gui_add_log_entry("PROCESS_KILLER", "ERROR", 
                             "Número de intento inválido en terminación de proceso");
            return -1;
    }
    
    // Registrar el intento que vamos a realizar
    char attempt_msg[512];
    snprintf(attempt_msg, sizeof(attempt_msg), 
             "Intento %d de terminar proceso '%s' (PID: %d) usando %s",
             attempt_number, process_name, pid, signal_name);
    gui_add_log_entry("PROCESS_KILLER", "INFO", attempt_msg);
    
    // Enviar la señal al proceso
    int result = kill(pid, signal_to_send);
    
    if (result == 0) {
        // La señal fue enviada exitosamente
        char success_msg[512];
        snprintf(success_msg, sizeof(success_msg), 
                 "Señal %s enviada exitosamente a proceso '%s' (PID: %d)",
                 signal_name, process_name, pid);
        gui_add_log_entry("PROCESS_KILLER", "INFO", success_msg);
        return 0;
    } else {
        // Error al enviar la señal
        char error_msg[512];
        const char* error_description = strerror(errno);
        
        if (errno == ESRCH) {
            snprintf(error_msg, sizeof(error_msg), 
                     "Proceso '%s' (PID: %d) ya no existe - posiblemente terminó por sí mismo",
                     process_name, pid);
            gui_add_log_entry("PROCESS_KILLER", "INFO", error_msg);
            return 1; // Código especial: proceso ya no existe
        } else if (errno == EPERM) {
            snprintf(error_msg, sizeof(error_msg), 
                     "Sin permisos para terminar proceso '%s' (PID: %d) - proceso del sistema o de otro usuario",
                     process_name, pid);
            gui_add_log_entry("PROCESS_KILLER", "ERROR", error_msg);
        } else {
            snprintf(error_msg, sizeof(error_msg), 
                     "Error al enviar %s a proceso '%s' (PID: %d): %s",
                     signal_name, process_name, pid, error_description);
            gui_add_log_entry("PROCESS_KILLER", "ERROR", error_msg);
        }
        
        return -1;
    }
}

// Callback que se ejecuta periódicamente para verificar si el proceso terminó
static gboolean check_process_termination_status(gpointer user_data) {
    ProcessTerminationContext *context = (ProcessTerminationContext*)user_data;
    
    if (!context) {
        gui_add_log_entry("PROCESS_KILLER", "ERROR", "Contexto de terminación nulo");
        return G_SOURCE_REMOVE; // Detener el timer
    }
    
    // Verificar si el proceso sigue vivo
    gboolean still_alive = is_process_still_alive(context->target_pid);
    
    if (!still_alive) {
        // ¡Éxito! El proceso terminó
        char success_msg[512];
        time_t total_time = time(NULL) - context->start_time;
        snprintf(success_msg, sizeof(success_msg), 
                 "✅ Proceso '%s' (PID: %d) terminado exitosamente después de %d intento(s) en %ld segundos",
                 context->process_name, context->target_pid, 
                 context->attempts_made, total_time);
        gui_add_log_entry("PROCESS_KILLER", "INFO", success_msg);
        
        // Remover el proceso de la GUI solo cuando realmente haya terminado
        if (context->iter_valid && process_list_store) {
            gtk_list_store_remove(process_list_store, &context->tree_iter);
        }
        
        // Limpiar el contexto y detener el timer
        free(context);
        return G_SOURCE_REMOVE;
    }
    
    // El proceso sigue vivo - verificar si debemos hacer otro intento
    time_t elapsed_time = time(NULL) - context->start_time;
    
    // Dar tiempo para que cada señal tenga efecto antes del siguiente intento
    const int WAIT_TIME_BETWEEN_ATTEMPTS = 3; // segundos
    const int MAX_WAIT_TIME = 15; // segundos máximos antes de abandonar
    
    if (elapsed_time >= MAX_WAIT_TIME) {
        // Hemos esperado demasiado - abandonar
        char failure_msg[512];
        snprintf(failure_msg, sizeof(failure_msg), 
                 "❌ Fallo al terminar proceso '%s' (PID: %d) después de %d intento(s) y %ld segundos",
                 context->process_name, context->target_pid, 
                 context->attempts_made, elapsed_time);
        gui_add_log_entry("PROCESS_KILLER", "ERROR", failure_msg);
        
        // Mostrar mensaje al usuario sobre el fallo
        char user_msg[512];
        snprintf(user_msg, sizeof(user_msg), 
                 "No se pudo terminar el proceso '%s' (PID: %d).\n"
                 "El proceso puede ser crítico del sistema o requerir permisos administrativos.",
                 context->process_name, context->target_pid);
        
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "%s", user_msg);
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        
        free(context);
        return G_SOURCE_REMOVE;
    }
    
    // Verificar si es tiempo de hacer otro intento
    if (elapsed_time >= (context->attempts_made * WAIT_TIME_BETWEEN_ATTEMPTS) && 
        context->attempts_made < 3) {
        
        context->attempts_made++;
        
        char retry_msg[256];
        snprintf(retry_msg, sizeof(retry_msg), 
                 "Proceso '%s' (PID: %d) sigue vivo después de %ld segundos - escalando a intento %d",
                 context->process_name, context->target_pid, elapsed_time, context->attempts_made);
        gui_add_log_entry("PROCESS_KILLER", "WARNING", retry_msg);
        
        // Hacer el siguiente intento con una señal más fuerte
        int result = attempt_process_termination(context->target_pid, 
                                                context->process_name, 
                                                context->attempts_made);
        
        if (result == 1) {
            // El proceso ya no existe (caso especial)
            free(context);
            return G_SOURCE_REMOVE;
        }
    }
    
    // Continuar verificando
    return G_SOURCE_CONTINUE;
}

// FUNCIÓN PRINCIPAL MEJORADA para terminar procesos
static void on_kill_process_clicked(GtkButton *button, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(process_tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gui_add_log_entry("PROCESS_KILLER", "WARNING", "No hay proceso seleccionado para terminar");
        return;
    }
    
    gint pid;
    gchar *name;
    gtk_tree_model_get(model, &iter,
                      COL_PROC_PID, &pid,
                      COL_PROC_NAME, &name,
                      -1);
    
    // Verificar que el proceso sigue existiendo antes de intentar terminarlo
    if (!is_process_still_alive(pid)) {
        char already_dead_msg[256];
        snprintf(already_dead_msg, sizeof(already_dead_msg), 
                 "El proceso '%s' (PID: %d) ya no está en ejecución", name, pid);
        gui_add_log_entry("PROCESS_KILLER", "INFO", already_dead_msg);
        
        // Remover de la GUI ya que no existe
        gtk_list_store_remove(process_list_store, &iter);
        g_free(name);
        return;
    }
    
    // Verificar si el sistema está pausado
    if (is_system_paused()) {
        GtkWidget *paused_dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                                         GTK_DIALOG_MODAL,
                                                         GTK_MESSAGE_INFO,
                                                         GTK_BUTTONS_OK,
                                                         "El sistema está pausado.\n\n"
                                                         "Para terminar procesos, primero reactive el monitoreo.");
        gtk_dialog_run(GTK_DIALOG(paused_dialog));
        gtk_widget_destroy(paused_dialog);
        g_free(name);
        return;
    }
    
    // === DIÁLOGO DE CONFIRMACIÓN CON INFORMACIÓN DE SEGURIDAD ===
    char confirmation_msg[1024];
    snprintf(confirmation_msg, sizeof(confirmation_msg),
             "¿Está seguro de que desea terminar el proceso '%s' (PID: %d)?\n\n"
             "ADVERTENCIA:\n"
             "• Terminar procesos del sistema puede causar inestabilidad\n"
             "• Los datos no guardados se perderán\n"
             "• Algunos procesos pueden requerir permisos administrativos\n\n"
             "MatCom Guard intentará terminación amigable primero, "
             "escalando a terminación forzada si es necesario.",
             name, pid);
    
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_WARNING,
                                              GTK_BUTTONS_YES_NO,
                                              "%s", confirmation_msg);
    
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirmar Terminación de Proceso");
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response != GTK_RESPONSE_YES) {
        gui_add_log_entry("PROCESS_KILLER", "INFO", "Terminación de proceso cancelada por el usuario");
        g_free(name);
        return;
    }
    
    // === INICIAR PROCESO DE TERMINACIÓN ASÍNCRONA ===
    
    // Crear contexto para el seguimiento asíncrono
    ProcessTerminationContext *context = malloc(sizeof(ProcessTerminationContext));
    if (!context) {
        gui_add_log_entry("PROCESS_KILLER", "ERROR", "Error al asignar memoria para contexto de terminación");
        g_free(name);
        return;
    }
    
    // Configurar el contexto
    context->target_pid = pid;
    strncpy(context->process_name, name, sizeof(context->process_name) - 1);
    context->process_name[sizeof(context->process_name) - 1] = '\0';
    context->attempts_made = 1;
    context->start_time = time(NULL);
    context->tree_iter = iter;
    context->iter_valid = TRUE;
    
    char start_msg[512];
    snprintf(start_msg, sizeof(start_msg), 
             "🎯 Iniciando terminación de proceso '%s' (PID: %d) con estrategia escalada",
             name, pid);
    gui_add_log_entry("PROCESS_KILLER", "INFO", start_msg);
    
    // Hacer el primer intento (SIGTERM amigable)
    int initial_result = attempt_process_termination(pid, name, 1);
    
    if (initial_result == 1) {
        // El proceso ya no existe
        char already_gone_msg[256];
        snprintf(already_gone_msg, sizeof(already_gone_msg), 
                 "Proceso '%s' (PID: %d) ya no existe - removiendo de la lista", name, pid);
        gui_add_log_entry("PROCESS_KILLER", "INFO", already_gone_msg);
        gtk_list_store_remove(process_list_store, &iter);
        free(context);
        g_free(name);
        return;
    } else if (initial_result == -1) {
        // Error en el primer intento
        free(context);
        g_free(name);
        return;
    }
    
    // Configurar timer para verificar el progreso cada segundo
    // Este timer verificará si el proceso terminó y escalará la terminación si es necesario
    g_timeout_add_seconds(1, check_process_termination_status, context);
    
    // Deshabilitar temporalmente el botón para evitar múltiples intentos simultáneos
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    
    // Re-habilitar el botón después de un tiempo razonable
    g_timeout_add_seconds(20, (GSourceFunc)gtk_widget_set_sensitive, 
                         GINT_TO_POINTER(TRUE));
    
    g_free(name);
}

// ===========================================================================
// FUNCIÓN AUXILIAR PARA VALIDAR PERMISOS DE TERMINACIÓN
// ===========================================================================

/**
 * Esta función verifica si tenemos permisos para terminar un proceso específico
 * antes de intentarlo. Esto ayuda a proporcionar mejor feedback al usuario.
 */
static gboolean can_terminate_process(pid_t pid, const char* process_name) {
    // Intentar enviar señal 0 (que no hace nada) para verificar permisos
    int result = kill(pid, 0);
    
    if (result == 0) {
        return TRUE; // Tenemos permisos
    } else if (errno == EPERM) {
        char perm_msg[512];
        snprintf(perm_msg, sizeof(perm_msg), 
                 "Sin permisos para terminar proceso '%s' (PID: %d) - proceso del sistema o de otro usuario",
                 process_name, pid);
        gui_add_log_entry("PROCESS_KILLER", "WARNING", perm_msg);
        return FALSE;
    } else if (errno == ESRCH) {
        char not_exist_msg[256];
        snprintf(not_exist_msg, sizeof(not_exist_msg), 
                 "Proceso '%s' (PID: %d) ya no existe", process_name, pid);
        gui_add_log_entry("PROCESS_KILLER", "INFO", not_exist_msg);
        return FALSE;
    }
    
    return FALSE; // Otros errores
}

// Callback para cambios en los umbrales
static void on_threshold_changed(GtkSpinButton *spin_button, gpointer data) {
    const char *type = (const char *)data;
    gdouble value = gtk_spin_button_get_value(spin_button);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Umbral de %s cambiado a %.0f%%", type, value);
    gui_add_log_entry("CONFIG", "INFO", log_msg);
    
    // Aquí se actualizaría la configuración real del backend
}

// Crear el panel principal de procesos
GtkWidget* create_process_panel() {
    // Contenedor principal
    process_panel_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(process_panel_container, 10);
    gtk_widget_set_margin_end(process_panel_container, 10);
    gtk_widget_set_margin_top(process_panel_container, 10);
    gtk_widget_set_margin_bottom(process_panel_container, 10);
    
    // Barra de herramientas
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span size='large' weight='bold'>⚡ Monitor de Procesos del Sistema</span>");
    gtk_box_pack_start(GTK_BOX(toolbar), title, FALSE, FALSE, 0);
    
    // Espaciador
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(toolbar), spacer, TRUE, TRUE, 0);
    
    // Botón de terminar proceso
    kill_process_button = gtk_button_new_with_label("❌ Terminar Proceso");
    gtk_widget_set_tooltip_text(kill_process_button, "Terminar el proceso seleccionado");
    gtk_widget_set_sensitive(kill_process_button, FALSE);
    g_signal_connect(kill_process_button, "clicked", G_CALLBACK(on_kill_process_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(toolbar), kill_process_button, FALSE, FALSE, 0);
    
    // Botón de escaneo
    scan_process_button = gtk_button_new_with_label("🔍 Escanear Procesos");
    gtk_widget_set_tooltip_text(scan_process_button, "Actualizar lista de procesos");
    g_signal_connect(scan_process_button, "clicked", G_CALLBACK(on_scan_processes_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(toolbar), scan_process_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(process_panel_container), toolbar, FALSE, FALSE, 0);
    
    // Separador
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(process_panel_container), separator1, FALSE, FALSE, 5);
    
    // Panel de configuración de umbrales
    GtkWidget *config_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(config_box, GTK_ALIGN_CENTER);
    
    // Umbral de CPU
    GtkWidget *cpu_label = gtk_label_new("Umbral CPU (%):");
    gtk_box_pack_start(GTK_BOX(config_box), cpu_label, FALSE, FALSE, 0);
    
    cpu_threshold_spin = gtk_spin_button_new_with_range(10.0, 100.0, 5.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cpu_threshold_spin), 70.0);
    gtk_widget_set_tooltip_text(cpu_threshold_spin, "Procesos que excedan este % de CPU serán marcados");
    g_signal_connect(cpu_threshold_spin, "value-changed", G_CALLBACK(on_threshold_changed), "CPU");
    gtk_box_pack_start(GTK_BOX(config_box), cpu_threshold_spin, FALSE, FALSE, 0);
    
    // Umbral de Memoria
    GtkWidget *mem_label = gtk_label_new("Umbral Memoria (%):");
    gtk_box_pack_start(GTK_BOX(config_box), mem_label, FALSE, FALSE, 0);
    
    mem_threshold_spin = gtk_spin_button_new_with_range(10.0, 100.0, 5.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mem_threshold_spin), 50.0);
    gtk_widget_set_tooltip_text(mem_threshold_spin, "Procesos que excedan este % de RAM serán marcados");
    g_signal_connect(mem_threshold_spin, "value-changed", G_CALLBACK(on_threshold_changed), "Memoria");
    gtk_box_pack_start(GTK_BOX(config_box), mem_threshold_spin, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(process_panel_container), config_box, FALSE, FALSE, 0);
    
    // Separador
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(process_panel_container), separator2, FALSE, FALSE, 5);
    
    // Contenedor horizontal para la lista y la información
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    // Crear el TreeView con scrolling
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 650, 300);
    
    // Crear el modelo de datos
    process_list_store = gtk_list_store_new(NUM_PROC_COLS,
                                           G_TYPE_STRING,  // Icon
                                           G_TYPE_INT,     // PID
                                           G_TYPE_STRING,  // Name
                                           G_TYPE_FLOAT,   // CPU usage
                                           G_TYPE_FLOAT,   // Memory usage
                                           G_TYPE_STRING,  // Status
                                           G_TYPE_STRING,  // Status color
                                           G_TYPE_STRING,  // CPU color
                                           G_TYPE_STRING); // Memory color
    
    process_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(process_list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(process_tree_view), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(process_tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(process_tree_view), COL_PROC_NAME);
    
    // Crear las columnas
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Columna de icono
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("", renderer,
                                                     "text", COL_PROC_ICON,
                                                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Columna de PID
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("PID", renderer,
                                                     "text", COL_PROC_PID,
                                                     NULL);
    gtk_tree_view_column_set_sort_column_id(column, COL_PROC_PID);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Columna de nombre del proceso
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Proceso", renderer,
                                                     "text", COL_PROC_NAME,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 200);
    gtk_tree_view_column_set_sort_column_id(column, COL_PROC_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Columna de uso de CPU con color
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("CPU %", renderer,
                                                     "text", COL_PROC_CPU_USAGE,
                                                     "foreground", COL_PROC_CPU_COLOR,
                                                     NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
        (GtkTreeCellDataFunc)format_cpu_column, NULL, NULL);
    gtk_tree_view_column_set_sort_column_id(column, COL_PROC_CPU_USAGE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Columna de uso de memoria con color
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Memoria %", renderer,
                                                     "text", COL_PROC_MEM_USAGE,
                                                     "foreground", COL_PROC_MEM_COLOR,
                                                     NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
        (GtkTreeCellDataFunc)format_mem_column, NULL, NULL);
    gtk_tree_view_column_set_sort_column_id(column, COL_PROC_MEM_USAGE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Columna de estado con color
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Estado", renderer,
                                                     "text", COL_PROC_STATUS,
                                                     "foreground", COL_PROC_STATUS_COLOR,
                                                     NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(process_tree_view), column);
    
    // Configurar selección
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(process_tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_process_selection_changed), NULL);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), process_tree_view);
    gtk_box_pack_start(GTK_BOX(content_box), scrolled_window, TRUE, TRUE, 0);
    
    // Panel de información detallada
    GtkWidget *info_frame = gtk_frame_new("Información del Proceso");
    gtk_widget_set_size_request(info_frame, 250, -1);
    
    process_info_label = gtk_label_new("Seleccione un proceso para ver detalles");
    gtk_label_set_line_wrap(GTK_LABEL(process_info_label), TRUE);
    gtk_widget_set_margin_start(process_info_label, 10);
    gtk_widget_set_margin_end(process_info_label, 10);
    gtk_widget_set_margin_top(process_info_label, 10);
    gtk_widget_set_margin_bottom(process_info_label, 10);
    gtk_label_set_xalign(GTK_LABEL(process_info_label), 0.0);
    
    gtk_container_add(GTK_CONTAINER(info_frame), process_info_label);
    gtk_box_pack_start(GTK_BOX(content_box), info_frame, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(process_panel_container), content_box, TRUE, TRUE, 0);
    
    // Barra de estado del panel
    GtkWidget *process_status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(process_status_bar, 5);
    
    GtkWidget *status_icon = gtk_label_new("ℹ️");
    gtk_box_pack_start(GTK_BOX(process_status_bar), status_icon, FALSE, FALSE, 0);
    
    GtkWidget *status_text = gtk_label_new("Los procesos marcados con 🚨 superan los umbrales configurados");
    gtk_widget_set_halign(status_text, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(process_status_bar), status_text, TRUE, TRUE, 0);
    
    gtk_box_pack_end(GTK_BOX(process_panel_container), process_status_bar, FALSE, FALSE, 0);
    
    return process_panel_container;
}

// Función auxiliar para formatear la columna de CPU
static void format_cpu_column(GtkTreeViewColumn *column,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data) {
    gfloat cpu_usage;
    gtk_tree_model_get(model, iter, COL_PROC_CPU_USAGE, &cpu_usage, -1);
    
    char text[32];
    snprintf(text, sizeof(text), "%.1f%%", cpu_usage);
    g_object_set(renderer, "text", text, NULL);
}

// Función auxiliar para formatear la columna de memoria
static void format_mem_column(GtkTreeViewColumn *column,
                             GtkCellRenderer *renderer,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data) {
    gfloat mem_usage;
    gtk_tree_model_get(model, iter, COL_PROC_MEM_USAGE, &mem_usage, -1);
    
    char text[32];
    snprintf(text, sizeof(text), "%.1f%%", mem_usage);
    g_object_set(renderer, "text", text, NULL);
}

// Función pública para actualizar un proceso
void gui_update_process(GUIProcess *process) {
    if (!process_list_store || !process) return;
    
    GtkTreeIter iter;
    gboolean found = FALSE;
    
    // Buscar si el proceso ya existe por PID
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(process_list_store), &iter)) {
        do {
            gint pid;
            gtk_tree_model_get(GTK_TREE_MODEL(process_list_store), &iter,
                             COL_PROC_PID, &pid, -1);
            
            if (pid == process->pid) {
                found = TRUE;
                break;
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(process_list_store), &iter));
    }
    
    // Si no existe, crear nueva entrada
    if (!found) {
        gtk_list_store_append(process_list_store, &iter);
    }
    
    // Obtener los umbrales actuales
    gdouble cpu_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(cpu_threshold_spin));
    gdouble mem_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(mem_threshold_spin));
    
    // Determinar estado y colores
    const char *status = "Normal";
    const char *status_color = "#4CAF50";
    
    if (process->is_suspicious) {
        status = "SOSPECHOSO";
        status_color = "#F44336";
    } else if (process->cpu_usage > cpu_threshold && process->mem_usage > mem_threshold) {
        status = "Alto CPU+RAM";
        status_color = "#F44336";
    } else if (process->cpu_usage > cpu_threshold) {
        status = "Alto CPU";
        status_color = "#FF9800";
    } else if (process->mem_usage > mem_threshold) {
        status = "Alta Memoria";
        status_color = "#FF9800";
    }
    
    const char *icon = get_process_icon(process->cpu_usage, process->mem_usage, process->is_suspicious);
    const char *cpu_color = get_usage_color(process->cpu_usage, cpu_threshold);
    const char *mem_color = get_usage_color(process->mem_usage, mem_threshold);
    
    // Actualizar los datos
    gtk_list_store_set(process_list_store, &iter,
                      COL_PROC_ICON, icon,
                      COL_PROC_PID, process->pid,
                      COL_PROC_NAME, process->name,
                      COL_PROC_CPU_USAGE, process->cpu_usage,
                      COL_PROC_MEM_USAGE, process->mem_usage,
                      COL_PROC_STATUS, status,
                      COL_PROC_STATUS_COLOR, status_color,
                      COL_PROC_CPU_COLOR, cpu_color,
                      COL_PROC_MEM_COLOR, mem_color,
                      -1);
    
    // Log del evento si es sospechoso
    if (process->is_suspicious || process->cpu_usage > cpu_threshold || process->mem_usage > mem_threshold) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), 
                "Proceso '%s' (PID: %d) - CPU: %.1f%%, RAM: %.1f%% - Estado: %s",
                process->name, process->pid, process->cpu_usage, process->mem_usage, status);
        gui_add_log_entry("PROCESS_MONITOR", 
                         process->is_suspicious ? "ALERT" : "WARNING", 
                         log_msg);
    }
}