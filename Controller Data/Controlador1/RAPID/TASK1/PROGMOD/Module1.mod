MODULE Module1
! Trayectoria segura
! IRB2600 - Orientación estable
! -----------------------
  PERS speeddata v12:=[12,500,5000,1000];
  PERS speeddata v17:=[17,500,5000,1000];
  PERS speeddata v7:=[7,500,5000,1000];
  !-----------------Variables------------
  PERS bool is_printing:=FALSE;
  PERS bool estado:=TRUE;
  PERS string comando:="";
  PERS string comn:="0";
  PERS string comn1:="0";
  PERS bool bucle;
  PERS bool Paro:=TRUE;
  PERS num temp_actual := 0; 
  PERS num set_point := 0;
  PERS num Vel_HMI := 30;
  !----------------------------------------------
    ! Declaración de la identidad de la interrupción
    VAR intnum int_ParoEmergencia;
    ! Variable que actuará como "Botón" en el FlexPendant
    PERS bool btn_reset := FALSE;
    PERS tooldata Extruder3:=[TRUE,[[17.4,-104.032,90.52],[0.5,0.5,0.5,-0.5]],[1,[0,0,1],[1,0,0,0],0,0,0]];
    TASK PERS wobjdata mesa1:=[FALSE,TRUE,"",[[1152.238466449,-151.368483103,237],[1,0,0,0]],[[0,0,0],[1,0,0,0]]];
    !-------------------------------------
    PROC main()
        btn_reset := FALSE;
        is_printing:=FALSE;
        comando:="";
        comn1:="0";
        comn:="0";
        IDelete int_ParoEmergencia;
        CONNECT int_ParoEmergencia WITH trap_Reiniciar;
        IPers btn_reset, int_ParoEmergencia;
        Paro:=TRUE;
        ConfigurarTriggers;
        WHILE Paro DO
            WaitUntil comn1="1";
            !WaitUntil comn="1";
            ! Acceleration and jerk/ramp (percentage)
            AccSet 20,20;
            ! Configurations
            ConfJ\On;
            ConfL\Off;
            p1004_1;
            ConfL\On;
            ConfJ\On;
    !-----------REINICIO DE VARIABLES--------------------
            comando:="ENFRIAR";
            WaitTime 0.1;
            comando:="";
            comn:="0";
            comn1:="0";
            bucle:=FALSE;
            Paro:=FALSE;
            Stop;
            ENDWHILE
        ENDPROC
    TRAP trap_Reiniciar
        comando:="";
        comn:="0";
        comn1:="0";
        Paro:=FALSE;
        btn_reset:=FALSE;
        ! Detiene el movimiento del robot inmediatamente
        StopMove;
        ! Borra la trayectoria restante que tenía calculada
        ClearPath;
        ! Mensaje para el usuario
        TPWrite "!!! PARO SOLICITADO - REINICIANDO !!!";
        WaitTime 1;
        ! Restablecemos el botón a falso para que no se active de nuevo solo
        ! Restablece el estado de movimiento (necesario tras StopMove)
        StartMove;
        ! Mueve el puntero de programa (PP) a la primera línea de Main
        ExitCycle;
    ENDTRAP
ENDMODULE