// Conexion a db
import moment from 'moment/moment.js';
import { connection } from '../db.js';
import sendResponse from '../libs/sendResponse.js';

export const getSensores = async (req, res) => {
    try {
        const periodos = req.body.periodos || null;
        let fechaUno = null;
        let fechaDos = null;

        if (periodos && Array.isArray(periodos) && periodos.length === 2) {
            fechaUno = periodos[0];
            fechaDos = periodos[1];
        } else {
            // Puedes poner valores por defecto o responder error
            return sendResponse(res, 400, true, 'Se requiere un rango de fechas válido');
        }

        const query = `
            WITH aperturas_y_cierres_emparejados AS (
                SELECT
                    t1.createdDate AS opened_at,
                    t1.mac,
                    (
                        SELECT MIN(t2.createdDate)
                        FROM detecciones AS t2
                        WHERE t2.mac = t1.mac
                            AND t2.createdDate > t1.createdDate
                            AND t2.createdDate <= ?
                            AND t2.estatus = 0  -- ahora cierre = 0
                    ) AS closed_at
                FROM detecciones AS t1
                WHERE t1.estatus = 1  -- ahora apertura = 1
                    AND t1.createdDate BETWEEN ? AND ?
            ),
            mayor_apertura AS (
                SELECT
                    mac,
                    opened_at AS fecha_mayor_apertura,
                    TIMESTAMPDIFF(SECOND, opened_at, closed_at) AS segundos_mayor_apertura
                FROM (
                    SELECT *,
                    ROW_NUMBER() OVER (PARTITION BY mac ORDER BY TIMESTAMPDIFF(SECOND, opened_at, closed_at) DESC) AS rn
                    FROM aperturas_y_cierres_emparejados
                    WHERE closed_at IS NOT NULL
                ) t
                WHERE rn = 1
            ),
            bateria_reciente AS (
                SELECT mac, bateria AS nivel_bateria_mas_reciente
                FROM (
                    SELECT *,
                    ROW_NUMBER() OVER (PARTITION BY mac ORDER BY createdDate DESC) AS rn
                    FROM detecciones
                    WHERE createdDate BETWEEN ? AND ?
                ) AS t
                WHERE rn = 1
            ),
            ultimo_estado AS (
                SELECT mac, estatus AS estado_reciente
                FROM (
                    SELECT *,
                    ROW_NUMBER() OVER (PARTITION BY mac ORDER BY createdDate DESC) AS rn
                    FROM detecciones
                    WHERE createdDate BETWEEN ? AND ?
                ) AS t
                WHERE rn = 1
            )

            SELECT
                a.mac,
                SUM(TIMESTAMPDIFF(SECOND, opened_at, closed_at)) AS tiempo_abierto_segundos, -- si quieres el total en segundos
                SEC_TO_TIME(SUM(TIMESTAMPDIFF(SECOND, opened_at, closed_at))) AS tiempo_abierto,
                TIME_FORMAT(SEC_TO_TIME(AVG(TIMESTAMPDIFF(SECOND, opened_at, closed_at))), '%H:%i:%s') AS tiempo_promedio,
                COUNT(a.opened_at) AS veces_abierto,
                b.nivel_bateria_mas_reciente AS bateria_actual,
                CASE e.estado_reciente
                    WHEN 1 THEN 'Abierto'
                    WHEN 0 THEN 'Cerrado'
                    ELSE 'Desconocido'
                END AS estatus_actual,
                SEC_TO_TIME(m.segundos_mayor_apertura) AS tiempo_mayor_apertura,
                DATE_FORMAT(m.fecha_mayor_apertura, '%Y-%m-%d %H:%i:%s') AS fecha_mayor_apertura,
                DATE_FORMAT(MIN(a.opened_at), '%Y-%m-%d %H:%i:%s') AS fecha_primer_apertura,
                DATE_FORMAT(MAX(a.opened_at), '%Y-%m-%d %H:%i:%s') AS fecha_ultima_apertura
            FROM aperturas_y_cierres_emparejados a
            LEFT JOIN bateria_reciente b ON a.mac = b.mac
            LEFT JOIN ultimo_estado e ON a.mac = e.mac
            LEFT JOIN mayor_apertura m ON a.mac = m.mac
            WHERE a.closed_at IS NOT NULL
            GROUP BY
                a.mac,
                b.nivel_bateria_mas_reciente,
                e.estado_reciente,
                m.segundos_mayor_apertura,
                m.fecha_mayor_apertura
            ORDER BY a.mac
            ;
        `;

        // Pasar los parámetros en orden
        const params = [fechaDos, fechaUno, fechaDos, fechaUno, fechaDos, fechaUno, fechaDos];

        const [rows] = await connection.query(query, params);
        return sendResponse(res, 200, false, 'Sensores obtenidos', rows);
    } catch (error) {
        console.error('Error al obtener sensores:', error.message);
        return sendResponse(res, 500, true, 'Error al obtener sensores');
    }
};
  
export const sendData = async (req, res) => {
    const { eventos } = req.body;

    if (!Array.isArray(eventos) || eventos.length === 0) return sendResponse(res, 500, true, "Faltan parametros.");

    const fecha = moment();

    for (const evento of eventos) {
        const { estatus, mac, bateria, tiempoEncendido } = evento;
        const tiempoEnSegundosAgregar = evento.segundosExtra || 0;

        // Valida que exista la mac
        if (!mac) continue;

        const fechaConSegundosAgregados = fecha.add(tiempoEnSegundosAgregar, 'seconds');

        const fechaYHora = fechaConSegundosAgregados.format('YYYY-MM-DD HH:mm:ss');

        try {
            let queryCampos = `createdDate, modifiedDate, mac, estatus, bateria, tiempoEncendido`;
            let queryValores = `'${fechaYHora}', '${fechaYHora}', ?, ?, ?, ?`;
            const values = [mac, estatus, bateria, tiempoEncendido];

            const query = `INSERT INTO detecciones (${queryCampos}) VALUES (${queryValores})`;

            await connection.query(query, values);
            console.log(`📥 Detección recibida → MAC: ${mac}, Estatus: ${estatus}, Batería: ${bateria}%, Tiempo: ${tiempoEnSegundosAgregar}s`);
        } catch (error) {
            // Si ocurre un error inesperado
            console.error("Error al insertar detección.", error.message);
            return sendResponse(res, 500, true, error.message);
        }
    }

    return sendResponse(res, 200, false, "Detecciones insertadas.");
}
