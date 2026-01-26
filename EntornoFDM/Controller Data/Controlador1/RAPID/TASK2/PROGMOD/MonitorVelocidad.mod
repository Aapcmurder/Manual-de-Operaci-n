
MODULE MonitorVelocidad
    
    !-----------------------Variables PERS compartidas entre HMI y Modulo1
    PERS bool estado:= FALSE;
    PERS string comando:= "";
    PERS bool Paro:= TRUE;
    PERS bool is_printing:= FALSE;
    !------------------------Variables PERS Compartidas entre TASK
    PERS string comn;
    PERS string comn1;
    PERS bool btn_reset := FALSE;
    
    !--------------------------Variables Locales de Lógica
    VAR bool last_printing_state := FALSE;
    ! Variables para controlar la saturación de envío-----------------------------------------------
    VAR clock reloj_envio;
    VAR num tiempo_actual := 0;
    VAR num ultima_velocidad_enviada := -1;
    ! Variable para leer el valor de la señal--------------------------------------------------------
    VAR num valor_velocidad_actual := 0;
    
    !-------------------------Variables Para leer_datos
    ! Variables para la recepción y desempaquetado de datos-------------------------------------------
    VAR rawbytes receive_string;
    VAR string s_mensaje_completo := "";
    ! Variables para los datos de telemetría (24 bytes)------------------------------------------------
    VAR string s_estado := "";
    VAR string s_temp := "";
    VAR string s_setpoint := "";
    
    PERS num temp_actual := 0;
    PERS num set_point := 0;
    PERS num Vel_HMI := 30;
    
    VAR num n_estado := 0;
    VAR num n_temp := 0;
    VAR num n_setpoint := 0;
    VAR bool ok_val := FALSE;
    PROC main()
        ! Iniciar reloj
        ClkReset reloj_envio;
        ClkStart reloj_envio;
        set_point:=0;
        Vel_HMI:=0;
        temp_actual:=0;
        abrircomuni; !Abre Conexión con Arudino
        WHILE Paro DO
            ! 1. Leer velocidad actual del robot
            valor_velocidad_actual := AOutput(VelTCP);
            Vel_HMI:=AOutput(VelTCP)*1000;
             
            ! 2. Escucha al Arduino (Seguridad y Telemetría)
            leer_datos; 
            !temp_actual := n_temp;
            !set_point := n_setpoint;
            ! --- LÓGICA DE IMPRESIÓN ---
            ! 3. LÓGICA DE ENVÍO DE VELOCIDAD
            ! CASO 1: FLANCO ASCENDENTE (Empieza a imprimir)
            
!            IF is_printing = TRUE AND last_printing_state = FALSE THEN
!                ! Flanco Ascendente: Empezar a imprimir
!                SocketSend sd1, \Str:="1 " + ValToStr(valor_velocidad_actual);
!                last_printing_state := TRUE;
            !                ultima_velocidad_enviada := valor_velocidad_actual;
            !                ClkReset reloj_envio;
            !                ClkStart reloj_envio;

!!           IF isprinting=TRUE THEN
!                 Flanco Ascendente: Empezar a imprimir
!                SocketSend sd1,\Str:="1 "+ValToStr(valor_velocidad_actual);
!                last_printing_state:=TRUE;
!                ultima_velocidad_enviada:=valor_velocidad_actual;
!                ClkReset reloj_envio;
!                ClkStart reloj_envio;
                
            ! CASO 2: MANTENIMIENTO (Sigue imprimiendo, verificamos si cambió la velocidad)
!            ELSEIF is_printing = TRUE AND last_printing_state = TRUE THEN
!                tiempo_actual := ClkRead(reloj_envio);
!                ! Solo actualizamos si la velocidad cambió Y ha pasado al menos 0.2s (200ms)
!                ! Esto evita saturar el Arduino
!                IF (Abs(valor_velocidad_actual - ultima_velocidad_enviada) > 0.1) AND (tiempo_actual > 0.2) THEN
!                     SocketSend sd1, \Str:="1 " + ValToStr(valor_velocidad_actual);
!                     ultima_velocidad_enviada := valor_velocidad_actual;
!                     ClkReset reloj_envio;
!                     ClkStart reloj_envio;
!                ENDIF

            ! CASO 3: FLANCO DESCENDENTE (Terminó de imprimir)
!            ELSEIF is_printing = FALSE AND last_printing_state = TRUE THEN
!                ! Enviamos STOP una sola vez
!                SocketSend sd1, \Str:="6 0";
!                last_printing_state := FALSE;
!            ENDIF
            
!            ELSEIF is_printing = FALSE AND last_printing_state = TRUE  THEN
!                ! Enviamos STOP una sola vez
!                !SocketSend sd1, \Str:="6 0";
!                last_printing_state := FALSE;
!            ENDIF
            
            ! 4. LÓGICA DE COMANDOS MANUALES (HMI)
            TEST comando
            CASE "START":
            !Lógica de Inicio
            SocketSend sd1,\Str:= "8 " + ValToStr(0);
            comando:="";
            CASE "STOP":
            !Stop del extrudor
            SocketSend sd1,\Str:= "6 " + ValToStr(0);
            !TPWrite "COMANDO: " + comando;
            comando:="";
            CASE "PRECALENTAR":
            !Calentar a 210 grados
            SocketSend sd1,\Str:= "2 " + ValToStr(0);
            set_point := 210;
            !TPWrite s_mensaje_completo;
            comando:="";
            CASE "ENFRIAR":
            !Endriar a 0 grados
            SocketSend sd1,\Str:= "3 " + ValToStr(0);
            set_point := 0;
            comando:="";
            CASE "EXTRUIR":
            !Empieza a extruir a una velocidad estalecida
            SocketSend sd1,\Str:= "4 " + ValToStr(0);
            comando:="";
            CASE "RETRAER":
            !Empieza a retrae a una velocidad estalecida
            SocketSend sd1,\Str:= "5 " + ValToStr(0);
            comando:="";
            CASE "FIJAR_TEMP":
            !Envia la Temperatura del Setpoint al Arduino 
            SocketSend sd1, \Str:= "7 " + ValToStr(set_point);
            TPWrite "Enviando nueva temperatura: " + ValToStr(set_point);
            comando:="";
            CASE "PARO":
            btn_reset:=TRUE;
            SocketSend sd1, \Str:= "9 " + ValToStr(0);
            EXITcycle;
            comando:="";
            CASE "EXTRUYE":
            SocketSend sd1, \Str:= "1 " + ValToStr(valor_velocidad_actual);
            TPWrite "COMANDO: " + comando;
            comando:="";
            ENDTEST
            !WaitTime 0.05;
        ENDWHILE
        cerrarcomuni;
        Paro:=TRUE;
        ENDPROC
ENDMODULE

