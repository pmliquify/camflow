import { useCallback, useEffect } from 'react';
import { getRuntimeStatus, setRuntimeStopped } from '../services/runtimeApi.js';

export default function useRemoteRuntimeStatus({ runtimes, localRuntimeId, runtimeBaseUrl, setRemoteRuntimeStatuses }) {

        const toggleRemoteRuntime = useCallback(async (runtime, shouldStop) => {
                const baseUrl = runtimeBaseUrl(runtime?.ip);
                if (!baseUrl) {
                        return false;
                }

                try {
                        const payload = await setRuntimeStopped(shouldStop, baseUrl);
                        const nextStatus = payload?.state === 'stopped' ? 'stopped' : 'running';
                        setRemoteRuntimeStatuses((current) => ({ ...current, [runtime.id]: nextStatus }));
                        return true;
                } catch (_) {
                        setRemoteRuntimeStatuses((current) => ({ ...current, [runtime.id]: 'down' }));
                        return false;
                }
        }, [runtimeBaseUrl]);

        const refreshRemoteRuntimeStatuses = useCallback(async () => {
                const remoteRuntimes = (runtimes || []).filter((runtime) => runtime.id !== localRuntimeId);
                if (remoteRuntimes.length === 0) {
                        setRemoteRuntimeStatuses((current) => (Object.keys(current).length === 0 ? current : {}));
                        return;
                }

                const updates = await Promise.all(remoteRuntimes.map(async (runtime) => {
                        const baseUrl = runtimeBaseUrl(runtime.ip);
                        if (!baseUrl) {
                                return [runtime.id, 'unknown'];
                        }

                        try {
                                const payload = await getRuntimeStatus(baseUrl);
                                return [runtime.id, payload?.state === 'stopped' ? 'stopped' : 'running'];
                        } catch (_) {
                                return [runtime.id, 'down'];
                        }
                }));

                const nextStatuses = Object.fromEntries(updates);
                setRemoteRuntimeStatuses((current) => {
                        const currentKeys = Object.keys(current);
                        const nextKeys = Object.keys(nextStatuses);
                        if (currentKeys.length !== nextKeys.length) {
                                return nextStatuses;
                        }
                        for (const key of nextKeys) {
                                if (current[key] !== nextStatuses[key]) {
                                        return nextStatuses;
                                }
                        }
                        return current;
                });
        }, [localRuntimeId, runtimeBaseUrl, runtimes]);

        useEffect(() => {
                void refreshRemoteRuntimeStatuses();
                const timer = setInterval(() => {
                        void refreshRemoteRuntimeStatuses();
                }, 5000);
                return () => clearInterval(timer);
        }, [refreshRemoteRuntimeStatuses]);

        return { toggleRemoteRuntime, refreshRemoteRuntimeStatuses };
}
