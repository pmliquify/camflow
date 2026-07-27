import React, { forwardRef, useEffect, useRef, useState } from 'react';

const ScrollArea = forwardRef(function ScrollArea({ className = '', stopWheelPropagation = true, transientScrollbar = true, onScroll, children, ...restProps }, ref) {
        const [isScrolling, setIsScrolling] = useState(false);
        const scrollResetTimerRef = useRef(null);

        useEffect(() => {
                return () => {
                        if (scrollResetTimerRef.current) {
                                window.clearTimeout(scrollResetTimerRef.current);
                        }
                };
        }, []);

        function handleScroll(event) {
                if (!transientScrollbar) {
                        return;
                }

                const target = event.currentTarget;
                if (target.scrollHeight <= target.clientHeight) {
                        return;
                }

                setIsScrolling(true);
                if (scrollResetTimerRef.current) {
                        window.clearTimeout(scrollResetTimerRef.current);
                }
                scrollResetTimerRef.current = window.setTimeout(() => {
                        setIsScrolling(false);
                        scrollResetTimerRef.current = null;
                }, 520);
        }

        return (
                <div
                        ref={ref}
                        {...restProps}
                        className={`${className}${transientScrollbar && isScrolling ? ' is-scrolling' : ''}`}
                        onWheelCapture={(event) => {
                                if (stopWheelPropagation) {
                                        event.stopPropagation();
                                }
                        }}
                        onScroll={(event) => {
                                handleScroll(event);
                                if (onScroll) {
                                        onScroll(event);
                                }
                        }}
                >
                        {children}
                </div>
        );
});

export default ScrollArea;
