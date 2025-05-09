typedef string cadena<256>;
program OBTENER_TIEMPO {
    version OBTENER_TIEMPO_VERS {
        int obtener_tiempo_servidor(cadena user, cadena op, cadena tiempo)=1;
    } = 1;
} = 99;