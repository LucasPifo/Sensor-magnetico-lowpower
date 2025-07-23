import { Router } from "express";
import { getFingerprint } from "../controllers/api.fingerprint.controller.js";

const router = Router();

// Esta ruta no es necesaria que se verifique el token
router.get('/fingerprint', getFingerprint);

export default router;