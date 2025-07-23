import { Router } from "express";
import { verifyToken } from "../middlewares/verifyToken.js";
import { sendData, getSensores } from "../controllers/api.sensorMagnetico.controller.js";

const router = Router();

router.post('/sensorMagnetico', verifyToken, sendData);
router.post('/sensorMagnetico/data', verifyToken, getSensores);

export default router;