import * as $protobuf from "protobufjs";
/** Namespace px. */
export namespace px {

    /** Namespace vispb. */
    namespace vispb {

        /** PXType enum. */
        enum PXType {
            PX_UNKNOWN = 0,
            PX_BOOLEAN = 1,
            PX_INT64 = 2,
            PX_FLOAT64 = 3,
            PX_STRING = 4,
            PX_SERVICE = 1000,
            PX_POD = 1001,
            PX_CONTAINER = 1002,
            PX_NAMESPACE = 1003,
            PX_NODE = 1004,
            PX_LIST = 2000,
            PX_STRING_LIST = 2001
        }

        /** Properties of a Vis. */
        interface IVis {

            /** Vis variables */
            variables?: (px.vispb.Vis.IVariable[]|null);

            /** Vis widgets */
            widgets?: (px.vispb.IWidget[]|null);

            /** Vis globalFuncs */
            globalFuncs?: (px.vispb.Vis.IGlobalFunc[]|null);
        }

        /** Represents a Vis. */
        class Vis implements IVis {

            /**
             * Constructs a new Vis.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IVis);

            /** Vis variables. */
            public variables: px.vispb.Vis.IVariable[];

            /** Vis widgets. */
            public widgets: px.vispb.IWidget[];

            /** Vis globalFuncs. */
            public globalFuncs: px.vispb.Vis.IGlobalFunc[];

            /**
             * Creates a new Vis instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Vis instance
             */
            public static create(properties?: px.vispb.IVis): px.vispb.Vis;

            /**
             * Encodes the specified Vis message. Does not implicitly {@link px.vispb.Vis.verify|verify} messages.
             * @param message Vis message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IVis, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Vis message, length delimited. Does not implicitly {@link px.vispb.Vis.verify|verify} messages.
             * @param message Vis message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IVis, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a Vis message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Vis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Vis;

            /**
             * Decodes a Vis message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Vis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Vis;

            /**
             * Verifies a Vis message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a Vis message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Vis
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.Vis;

            /**
             * Creates a plain object from a Vis message. Also converts values to other types if specified.
             * @param message Vis
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.Vis, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Vis to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace Vis {

            /** Properties of a Variable. */
            interface IVariable {

                /** Variable name */
                name?: (string|null);

                /** Variable type */
                type?: (px.vispb.PXType|null);

                /** Variable defaultValue */
                defaultValue?: (google.protobuf.IStringValue|null);

                /** Variable description */
                description?: (string|null);

                /** Variable validValues */
                validValues?: (string[]|null);
            }

            /** Represents a Variable. */
            class Variable implements IVariable {

                /**
                 * Constructs a new Variable.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Vis.IVariable);

                /** Variable name. */
                public name: string;

                /** Variable type. */
                public type: px.vispb.PXType;

                /** Variable defaultValue. */
                public defaultValue?: (google.protobuf.IStringValue|null);

                /** Variable description. */
                public description: string;

                /** Variable validValues. */
                public validValues: string[];

                /**
                 * Creates a new Variable instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Variable instance
                 */
                public static create(properties?: px.vispb.Vis.IVariable): px.vispb.Vis.Variable;

                /**
                 * Encodes the specified Variable message. Does not implicitly {@link px.vispb.Vis.Variable.verify|verify} messages.
                 * @param message Variable message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Vis.IVariable, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Variable message, length delimited. Does not implicitly {@link px.vispb.Vis.Variable.verify|verify} messages.
                 * @param message Variable message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Vis.IVariable, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Variable message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Variable
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Vis.Variable;

                /**
                 * Decodes a Variable message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Variable
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Vis.Variable;

                /**
                 * Verifies a Variable message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Variable message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Variable
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Vis.Variable;

                /**
                 * Creates a plain object from a Variable message. Also converts values to other types if specified.
                 * @param message Variable
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Vis.Variable, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Variable to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }

            /** Properties of a GlobalFunc. */
            interface IGlobalFunc {

                /** GlobalFunc outputName */
                outputName?: (string|null);

                /** GlobalFunc func */
                func?: (px.vispb.Widget.IFunc|null);
            }

            /** Represents a GlobalFunc. */
            class GlobalFunc implements IGlobalFunc {

                /**
                 * Constructs a new GlobalFunc.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Vis.IGlobalFunc);

                /** GlobalFunc outputName. */
                public outputName: string;

                /** GlobalFunc func. */
                public func?: (px.vispb.Widget.IFunc|null);

                /**
                 * Creates a new GlobalFunc instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns GlobalFunc instance
                 */
                public static create(properties?: px.vispb.Vis.IGlobalFunc): px.vispb.Vis.GlobalFunc;

                /**
                 * Encodes the specified GlobalFunc message. Does not implicitly {@link px.vispb.Vis.GlobalFunc.verify|verify} messages.
                 * @param message GlobalFunc message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Vis.IGlobalFunc, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified GlobalFunc message, length delimited. Does not implicitly {@link px.vispb.Vis.GlobalFunc.verify|verify} messages.
                 * @param message GlobalFunc message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Vis.IGlobalFunc, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a GlobalFunc message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns GlobalFunc
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Vis.GlobalFunc;

                /**
                 * Decodes a GlobalFunc message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns GlobalFunc
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Vis.GlobalFunc;

                /**
                 * Verifies a GlobalFunc message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a GlobalFunc message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns GlobalFunc
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Vis.GlobalFunc;

                /**
                 * Creates a plain object from a GlobalFunc message. Also converts values to other types if specified.
                 * @param message GlobalFunc
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Vis.GlobalFunc, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this GlobalFunc to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }
        }

        /** Properties of a Widget. */
        interface IWidget {

            /** Widget name */
            name?: (string|null);

            /** Widget position */
            position?: (px.vispb.Widget.IPosition|null);

            /** Widget func */
            func?: (px.vispb.Widget.IFunc|null);

            /** Widget globalFuncOutputName */
            globalFuncOutputName?: (string|null);

            /** Widget displaySpec */
            displaySpec?: (google.protobuf.IAny|null);
        }

        /** Represents a Widget. */
        class Widget implements IWidget {

            /**
             * Constructs a new Widget.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IWidget);

            /** Widget name. */
            public name: string;

            /** Widget position. */
            public position?: (px.vispb.Widget.IPosition|null);

            /** Widget func. */
            public func?: (px.vispb.Widget.IFunc|null);

            /** Widget globalFuncOutputName. */
            public globalFuncOutputName?: (string|null);

            /** Widget displaySpec. */
            public displaySpec?: (google.protobuf.IAny|null);

            /** Widget funcOrRef. */
            public funcOrRef?: ("func"|"globalFuncOutputName");

            /**
             * Creates a new Widget instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Widget instance
             */
            public static create(properties?: px.vispb.IWidget): px.vispb.Widget;

            /**
             * Encodes the specified Widget message. Does not implicitly {@link px.vispb.Widget.verify|verify} messages.
             * @param message Widget message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IWidget, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Widget message, length delimited. Does not implicitly {@link px.vispb.Widget.verify|verify} messages.
             * @param message Widget message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IWidget, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a Widget message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Widget
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Widget;

            /**
             * Decodes a Widget message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Widget
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Widget;

            /**
             * Verifies a Widget message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a Widget message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Widget
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.Widget;

            /**
             * Creates a plain object from a Widget message. Also converts values to other types if specified.
             * @param message Widget
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.Widget, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Widget to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace Widget {

            /** Properties of a Position. */
            interface IPosition {

                /** Position x */
                x?: (number|null);

                /** Position y */
                y?: (number|null);

                /** Position w */
                w?: (number|null);

                /** Position h */
                h?: (number|null);
            }

            /** Represents a Position. */
            class Position implements IPosition {

                /**
                 * Constructs a new Position.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Widget.IPosition);

                /** Position x. */
                public x: number;

                /** Position y. */
                public y: number;

                /** Position w. */
                public w: number;

                /** Position h. */
                public h: number;

                /**
                 * Creates a new Position instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Position instance
                 */
                public static create(properties?: px.vispb.Widget.IPosition): px.vispb.Widget.Position;

                /**
                 * Encodes the specified Position message. Does not implicitly {@link px.vispb.Widget.Position.verify|verify} messages.
                 * @param message Position message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Widget.IPosition, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Position message, length delimited. Does not implicitly {@link px.vispb.Widget.Position.verify|verify} messages.
                 * @param message Position message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Widget.IPosition, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Position message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Position
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Widget.Position;

                /**
                 * Decodes a Position message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Position
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Widget.Position;

                /**
                 * Verifies a Position message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Position message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Position
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Widget.Position;

                /**
                 * Creates a plain object from a Position message. Also converts values to other types if specified.
                 * @param message Position
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Widget.Position, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Position to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }

            /** Properties of a Func. */
            interface IFunc {

                /** Func name */
                name?: (string|null);

                /** Func args */
                args?: (px.vispb.Widget.Func.IFuncArg[]|null);
            }

            /** Represents a Func. */
            class Func implements IFunc {

                /**
                 * Constructs a new Func.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Widget.IFunc);

                /** Func name. */
                public name: string;

                /** Func args. */
                public args: px.vispb.Widget.Func.IFuncArg[];

                /**
                 * Creates a new Func instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Func instance
                 */
                public static create(properties?: px.vispb.Widget.IFunc): px.vispb.Widget.Func;

                /**
                 * Encodes the specified Func message. Does not implicitly {@link px.vispb.Widget.Func.verify|verify} messages.
                 * @param message Func message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Widget.IFunc, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Func message, length delimited. Does not implicitly {@link px.vispb.Widget.Func.verify|verify} messages.
                 * @param message Func message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Widget.IFunc, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Func message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Func
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Widget.Func;

                /**
                 * Decodes a Func message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Func
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Widget.Func;

                /**
                 * Verifies a Func message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Func message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Func
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Widget.Func;

                /**
                 * Creates a plain object from a Func message. Also converts values to other types if specified.
                 * @param message Func
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Widget.Func, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Func to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }

            namespace Func {

                /** Properties of a FuncArg. */
                interface IFuncArg {

                    /** FuncArg name */
                    name?: (string|null);

                    /** FuncArg value */
                    value?: (string|null);

                    /** FuncArg variable */
                    variable?: (string|null);
                }

                /** Represents a FuncArg. */
                class FuncArg implements IFuncArg {

                    /**
                     * Constructs a new FuncArg.
                     * @param [properties] Properties to set
                     */
                    constructor(properties?: px.vispb.Widget.Func.IFuncArg);

                    /** FuncArg name. */
                    public name: string;

                    /** FuncArg value. */
                    public value?: (string|null);

                    /** FuncArg variable. */
                    public variable?: (string|null);

                    /** FuncArg input. */
                    public input?: ("value"|"variable");

                    /**
                     * Creates a new FuncArg instance using the specified properties.
                     * @param [properties] Properties to set
                     * @returns FuncArg instance
                     */
                    public static create(properties?: px.vispb.Widget.Func.IFuncArg): px.vispb.Widget.Func.FuncArg;

                    /**
                     * Encodes the specified FuncArg message. Does not implicitly {@link px.vispb.Widget.Func.FuncArg.verify|verify} messages.
                     * @param message FuncArg message or plain object to encode
                     * @param [writer] Writer to encode to
                     * @returns Writer
                     */
                    public static encode(message: px.vispb.Widget.Func.IFuncArg, writer?: $protobuf.Writer): $protobuf.Writer;

                    /**
                     * Encodes the specified FuncArg message, length delimited. Does not implicitly {@link px.vispb.Widget.Func.FuncArg.verify|verify} messages.
                     * @param message FuncArg message or plain object to encode
                     * @param [writer] Writer to encode to
                     * @returns Writer
                     */
                    public static encodeDelimited(message: px.vispb.Widget.Func.IFuncArg, writer?: $protobuf.Writer): $protobuf.Writer;

                    /**
                     * Decodes a FuncArg message from the specified reader or buffer.
                     * @param reader Reader or buffer to decode from
                     * @param [length] Message length if known beforehand
                     * @returns FuncArg
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Widget.Func.FuncArg;

                    /**
                     * Decodes a FuncArg message from the specified reader or buffer, length delimited.
                     * @param reader Reader or buffer to decode from
                     * @returns FuncArg
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Widget.Func.FuncArg;

                    /**
                     * Verifies a FuncArg message.
                     * @param message Plain object to verify
                     * @returns `null` if valid, otherwise the reason why it is not
                     */
                    public static verify(message: { [k: string]: any }): (string|null);

                    /**
                     * Creates a FuncArg message from a plain object. Also converts values to their respective internal types.
                     * @param object Plain object
                     * @returns FuncArg
                     */
                    public static fromObject(object: { [k: string]: any }): px.vispb.Widget.Func.FuncArg;

                    /**
                     * Creates a plain object from a FuncArg message. Also converts values to other types if specified.
                     * @param message FuncArg
                     * @param [options] Conversion options
                     * @returns Plain object
                     */
                    public static toObject(message: px.vispb.Widget.Func.FuncArg, options?: $protobuf.IConversionOptions): { [k: string]: any };

                    /**
                     * Converts this FuncArg to JSON.
                     * @returns JSON object
                     */
                    public toJSON(): { [k: string]: any };
                }
            }
        }

        /** Properties of an Axis. */
        interface IAxis {

            /** Axis label */
            label?: (string|null);
        }

        /** Represents an Axis. */
        class Axis implements IAxis {

            /**
             * Constructs a new Axis.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IAxis);

            /** Axis label. */
            public label: string;

            /**
             * Creates a new Axis instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Axis instance
             */
            public static create(properties?: px.vispb.IAxis): px.vispb.Axis;

            /**
             * Encodes the specified Axis message. Does not implicitly {@link px.vispb.Axis.verify|verify} messages.
             * @param message Axis message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IAxis, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Axis message, length delimited. Does not implicitly {@link px.vispb.Axis.verify|verify} messages.
             * @param message Axis message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IAxis, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes an Axis message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Axis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Axis;

            /**
             * Decodes an Axis message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Axis
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Axis;

            /**
             * Verifies an Axis message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates an Axis message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Axis
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.Axis;

            /**
             * Creates a plain object from an Axis message. Also converts values to other types if specified.
             * @param message Axis
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.Axis, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Axis to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a BarChart. */
        interface IBarChart {

            /** BarChart bar */
            bar?: (px.vispb.BarChart.IBar|null);

            /** BarChart title */
            title?: (string|null);

            /** BarChart xAxis */
            xAxis?: (px.vispb.IAxis|null);

            /** BarChart yAxis */
            yAxis?: (px.vispb.IAxis|null);
        }

        /** Represents a BarChart. */
        class BarChart implements IBarChart {

            /**
             * Constructs a new BarChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IBarChart);

            /** BarChart bar. */
            public bar?: (px.vispb.BarChart.IBar|null);

            /** BarChart title. */
            public title: string;

            /** BarChart xAxis. */
            public xAxis?: (px.vispb.IAxis|null);

            /** BarChart yAxis. */
            public yAxis?: (px.vispb.IAxis|null);

            /**
             * Creates a new BarChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns BarChart instance
             */
            public static create(properties?: px.vispb.IBarChart): px.vispb.BarChart;

            /**
             * Encodes the specified BarChart message. Does not implicitly {@link px.vispb.BarChart.verify|verify} messages.
             * @param message BarChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IBarChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified BarChart message, length delimited. Does not implicitly {@link px.vispb.BarChart.verify|verify} messages.
             * @param message BarChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IBarChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a BarChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns BarChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.BarChart;

            /**
             * Decodes a BarChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns BarChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.BarChart;

            /**
             * Verifies a BarChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a BarChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns BarChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.BarChart;

            /**
             * Creates a plain object from a BarChart message. Also converts values to other types if specified.
             * @param message BarChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.BarChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this BarChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace BarChart {

            /** Properties of a Bar. */
            interface IBar {

                /** Bar value */
                value?: (string|null);

                /** Bar label */
                label?: (string|null);

                /** Bar stackBy */
                stackBy?: (string|null);

                /** Bar groupBy */
                groupBy?: (string|null);

                /** Bar horizontal */
                horizontal?: (boolean|null);
            }

            /** Represents a Bar. */
            class Bar implements IBar {

                /**
                 * Constructs a new Bar.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.BarChart.IBar);

                /** Bar value. */
                public value: string;

                /** Bar label. */
                public label: string;

                /** Bar stackBy. */
                public stackBy: string;

                /** Bar groupBy. */
                public groupBy: string;

                /** Bar horizontal. */
                public horizontal: boolean;

                /**
                 * Creates a new Bar instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Bar instance
                 */
                public static create(properties?: px.vispb.BarChart.IBar): px.vispb.BarChart.Bar;

                /**
                 * Encodes the specified Bar message. Does not implicitly {@link px.vispb.BarChart.Bar.verify|verify} messages.
                 * @param message Bar message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.BarChart.IBar, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Bar message, length delimited. Does not implicitly {@link px.vispb.BarChart.Bar.verify|verify} messages.
                 * @param message Bar message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.BarChart.IBar, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Bar message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Bar
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.BarChart.Bar;

                /**
                 * Decodes a Bar message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Bar
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.BarChart.Bar;

                /**
                 * Verifies a Bar message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Bar message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Bar
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.BarChart.Bar;

                /**
                 * Creates a plain object from a Bar message. Also converts values to other types if specified.
                 * @param message Bar
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.BarChart.Bar, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Bar to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }
        }

        /** Properties of a PieChart. */
        interface IPieChart {

            /** PieChart label */
            label?: (string|null);

            /** PieChart value */
            value?: (string|null);

            /** PieChart title */
            title?: (string|null);
        }

        /** Represents a PieChart. */
        class PieChart implements IPieChart {

            /**
             * Constructs a new PieChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IPieChart);

            /** PieChart label. */
            public label: string;

            /** PieChart value. */
            public value: string;

            /** PieChart title. */
            public title: string;

            /**
             * Creates a new PieChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns PieChart instance
             */
            public static create(properties?: px.vispb.IPieChart): px.vispb.PieChart;

            /**
             * Encodes the specified PieChart message. Does not implicitly {@link px.vispb.PieChart.verify|verify} messages.
             * @param message PieChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IPieChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified PieChart message, length delimited. Does not implicitly {@link px.vispb.PieChart.verify|verify} messages.
             * @param message PieChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IPieChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a PieChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns PieChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.PieChart;

            /**
             * Decodes a PieChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns PieChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.PieChart;

            /**
             * Verifies a PieChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a PieChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns PieChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.PieChart;

            /**
             * Creates a plain object from a PieChart message. Also converts values to other types if specified.
             * @param message PieChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.PieChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this PieChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a HistogramChart. */
        interface IHistogramChart {

            /** HistogramChart histogram */
            histogram?: (px.vispb.HistogramChart.IHistogram|null);

            /** HistogramChart title */
            title?: (string|null);

            /** HistogramChart xAxis */
            xAxis?: (px.vispb.IAxis|null);

            /** HistogramChart yAxis */
            yAxis?: (px.vispb.IAxis|null);
        }

        /** Represents a HistogramChart. */
        class HistogramChart implements IHistogramChart {

            /**
             * Constructs a new HistogramChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IHistogramChart);

            /** HistogramChart histogram. */
            public histogram?: (px.vispb.HistogramChart.IHistogram|null);

            /** HistogramChart title. */
            public title: string;

            /** HistogramChart xAxis. */
            public xAxis?: (px.vispb.IAxis|null);

            /** HistogramChart yAxis. */
            public yAxis?: (px.vispb.IAxis|null);

            /**
             * Creates a new HistogramChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns HistogramChart instance
             */
            public static create(properties?: px.vispb.IHistogramChart): px.vispb.HistogramChart;

            /**
             * Encodes the specified HistogramChart message. Does not implicitly {@link px.vispb.HistogramChart.verify|verify} messages.
             * @param message HistogramChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IHistogramChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified HistogramChart message, length delimited. Does not implicitly {@link px.vispb.HistogramChart.verify|verify} messages.
             * @param message HistogramChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IHistogramChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a HistogramChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns HistogramChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.HistogramChart;

            /**
             * Decodes a HistogramChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns HistogramChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.HistogramChart;

            /**
             * Verifies a HistogramChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a HistogramChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns HistogramChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.HistogramChart;

            /**
             * Creates a plain object from a HistogramChart message. Also converts values to other types if specified.
             * @param message HistogramChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.HistogramChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this HistogramChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace HistogramChart {

            /** Properties of a Histogram. */
            interface IHistogram {

                /** Histogram value */
                value?: (string|null);

                /** Histogram maxbins */
                maxbins?: (number|Long|null);

                /** Histogram minstep */
                minstep?: (number|null);

                /** Histogram horizontal */
                horizontal?: (boolean|null);

                /** Histogram prebinCount */
                prebinCount?: (string|null);
            }

            /** Represents a Histogram. */
            class Histogram implements IHistogram {

                /**
                 * Constructs a new Histogram.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.HistogramChart.IHistogram);

                /** Histogram value. */
                public value: string;

                /** Histogram maxbins. */
                public maxbins: (number|Long);

                /** Histogram minstep. */
                public minstep: number;

                /** Histogram horizontal. */
                public horizontal: boolean;

                /** Histogram prebinCount. */
                public prebinCount: string;

                /**
                 * Creates a new Histogram instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Histogram instance
                 */
                public static create(properties?: px.vispb.HistogramChart.IHistogram): px.vispb.HistogramChart.Histogram;

                /**
                 * Encodes the specified Histogram message. Does not implicitly {@link px.vispb.HistogramChart.Histogram.verify|verify} messages.
                 * @param message Histogram message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.HistogramChart.IHistogram, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Histogram message, length delimited. Does not implicitly {@link px.vispb.HistogramChart.Histogram.verify|verify} messages.
                 * @param message Histogram message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.HistogramChart.IHistogram, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Histogram message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Histogram
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.HistogramChart.Histogram;

                /**
                 * Decodes a Histogram message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Histogram
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.HistogramChart.Histogram;

                /**
                 * Verifies a Histogram message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Histogram message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Histogram
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.HistogramChart.Histogram;

                /**
                 * Creates a plain object from a Histogram message. Also converts values to other types if specified.
                 * @param message Histogram
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.HistogramChart.Histogram, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Histogram to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }
        }

        /** Properties of a GaugeChart. */
        interface IGaugeChart {

            /** GaugeChart value */
            value?: (string|null);

            /** GaugeChart title */
            title?: (string|null);
        }

        /** Represents a GaugeChart. */
        class GaugeChart implements IGaugeChart {

            /**
             * Constructs a new GaugeChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IGaugeChart);

            /** GaugeChart value. */
            public value: string;

            /** GaugeChart title. */
            public title: string;

            /**
             * Creates a new GaugeChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns GaugeChart instance
             */
            public static create(properties?: px.vispb.IGaugeChart): px.vispb.GaugeChart;

            /**
             * Encodes the specified GaugeChart message. Does not implicitly {@link px.vispb.GaugeChart.verify|verify} messages.
             * @param message GaugeChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IGaugeChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified GaugeChart message, length delimited. Does not implicitly {@link px.vispb.GaugeChart.verify|verify} messages.
             * @param message GaugeChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IGaugeChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a GaugeChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns GaugeChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.GaugeChart;

            /**
             * Decodes a GaugeChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns GaugeChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.GaugeChart;

            /**
             * Verifies a GaugeChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a GaugeChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns GaugeChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.GaugeChart;

            /**
             * Creates a plain object from a GaugeChart message. Also converts values to other types if specified.
             * @param message GaugeChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.GaugeChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this GaugeChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a TimeseriesChart. */
        interface ITimeseriesChart {

            /** TimeseriesChart timeseries */
            timeseries?: (px.vispb.TimeseriesChart.ITimeseries[]|null);

            /** TimeseriesChart title */
            title?: (string|null);

            /** TimeseriesChart xAxis */
            xAxis?: (px.vispb.IAxis|null);

            /** TimeseriesChart yAxis */
            yAxis?: (px.vispb.IAxis|null);
        }

        /** Represents a TimeseriesChart. */
        class TimeseriesChart implements ITimeseriesChart {

            /**
             * Constructs a new TimeseriesChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.ITimeseriesChart);

            /** TimeseriesChart timeseries. */
            public timeseries: px.vispb.TimeseriesChart.ITimeseries[];

            /** TimeseriesChart title. */
            public title: string;

            /** TimeseriesChart xAxis. */
            public xAxis?: (px.vispb.IAxis|null);

            /** TimeseriesChart yAxis. */
            public yAxis?: (px.vispb.IAxis|null);

            /**
             * Creates a new TimeseriesChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns TimeseriesChart instance
             */
            public static create(properties?: px.vispb.ITimeseriesChart): px.vispb.TimeseriesChart;

            /**
             * Encodes the specified TimeseriesChart message. Does not implicitly {@link px.vispb.TimeseriesChart.verify|verify} messages.
             * @param message TimeseriesChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.ITimeseriesChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified TimeseriesChart message, length delimited. Does not implicitly {@link px.vispb.TimeseriesChart.verify|verify} messages.
             * @param message TimeseriesChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.ITimeseriesChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a TimeseriesChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns TimeseriesChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.TimeseriesChart;

            /**
             * Decodes a TimeseriesChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns TimeseriesChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.TimeseriesChart;

            /**
             * Verifies a TimeseriesChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a TimeseriesChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns TimeseriesChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.TimeseriesChart;

            /**
             * Creates a plain object from a TimeseriesChart message. Also converts values to other types if specified.
             * @param message TimeseriesChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.TimeseriesChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this TimeseriesChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace TimeseriesChart {

            /** Properties of a Timeseries. */
            interface ITimeseries {

                /** Timeseries value */
                value?: (string|null);

                /** Timeseries series */
                series?: (string|null);

                /** Timeseries stackBySeries */
                stackBySeries?: (boolean|null);

                /** Timeseries mode */
                mode?: (px.vispb.TimeseriesChart.Timeseries.Mode|null);
            }

            /** Represents a Timeseries. */
            class Timeseries implements ITimeseries {

                /**
                 * Constructs a new Timeseries.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.TimeseriesChart.ITimeseries);

                /** Timeseries value. */
                public value: string;

                /** Timeseries series. */
                public series: string;

                /** Timeseries stackBySeries. */
                public stackBySeries: boolean;

                /** Timeseries mode. */
                public mode: px.vispb.TimeseriesChart.Timeseries.Mode;

                /**
                 * Creates a new Timeseries instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Timeseries instance
                 */
                public static create(properties?: px.vispb.TimeseriesChart.ITimeseries): px.vispb.TimeseriesChart.Timeseries;

                /**
                 * Encodes the specified Timeseries message. Does not implicitly {@link px.vispb.TimeseriesChart.Timeseries.verify|verify} messages.
                 * @param message Timeseries message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.TimeseriesChart.ITimeseries, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Timeseries message, length delimited. Does not implicitly {@link px.vispb.TimeseriesChart.Timeseries.verify|verify} messages.
                 * @param message Timeseries message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.TimeseriesChart.ITimeseries, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Timeseries message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Timeseries
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.TimeseriesChart.Timeseries;

                /**
                 * Decodes a Timeseries message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Timeseries
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.TimeseriesChart.Timeseries;

                /**
                 * Verifies a Timeseries message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Timeseries message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Timeseries
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.TimeseriesChart.Timeseries;

                /**
                 * Creates a plain object from a Timeseries message. Also converts values to other types if specified.
                 * @param message Timeseries
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.TimeseriesChart.Timeseries, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Timeseries to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }

            namespace Timeseries {

                /** Mode enum. */
                enum Mode {
                    MODE_UNKNOWN = 0,
                    MODE_LINE = 2,
                    MODE_POINT = 3,
                    MODE_AREA = 4
                }
            }
        }

        /** Properties of a StatChart. */
        interface IStatChart {

            /** StatChart stat */
            stat?: (px.vispb.StatChart.IStat|null);

            /** StatChart title */
            title?: (string|null);
        }

        /** Represents a StatChart. */
        class StatChart implements IStatChart {

            /**
             * Constructs a new StatChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IStatChart);

            /** StatChart stat. */
            public stat?: (px.vispb.StatChart.IStat|null);

            /** StatChart title. */
            public title: string;

            /**
             * Creates a new StatChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns StatChart instance
             */
            public static create(properties?: px.vispb.IStatChart): px.vispb.StatChart;

            /**
             * Encodes the specified StatChart message. Does not implicitly {@link px.vispb.StatChart.verify|verify} messages.
             * @param message StatChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IStatChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified StatChart message, length delimited. Does not implicitly {@link px.vispb.StatChart.verify|verify} messages.
             * @param message StatChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IStatChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a StatChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns StatChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.StatChart;

            /**
             * Decodes a StatChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns StatChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.StatChart;

            /**
             * Verifies a StatChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a StatChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns StatChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.StatChart;

            /**
             * Creates a plain object from a StatChart message. Also converts values to other types if specified.
             * @param message StatChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.StatChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this StatChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace StatChart {

            /** Properties of a Stat. */
            interface IStat {

                /** Stat value */
                value?: (string|null);
            }

            /** Represents a Stat. */
            class Stat implements IStat {

                /**
                 * Constructs a new Stat.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.StatChart.IStat);

                /** Stat value. */
                public value: string;

                /**
                 * Creates a new Stat instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns Stat instance
                 */
                public static create(properties?: px.vispb.StatChart.IStat): px.vispb.StatChart.Stat;

                /**
                 * Encodes the specified Stat message. Does not implicitly {@link px.vispb.StatChart.Stat.verify|verify} messages.
                 * @param message Stat message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.StatChart.IStat, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified Stat message, length delimited. Does not implicitly {@link px.vispb.StatChart.Stat.verify|verify} messages.
                 * @param message Stat message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.StatChart.IStat, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes a Stat message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns Stat
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.StatChart.Stat;

                /**
                 * Decodes a Stat message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns Stat
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.StatChart.Stat;

                /**
                 * Verifies a Stat message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates a Stat message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns Stat
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.StatChart.Stat;

                /**
                 * Creates a plain object from a Stat message. Also converts values to other types if specified.
                 * @param message Stat
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.StatChart.Stat, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this Stat to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }
        }

        /** Properties of a TextChart. */
        interface ITextChart {

            /** TextChart body */
            body?: (string|null);
        }

        /** Represents a TextChart. */
        class TextChart implements ITextChart {

            /**
             * Constructs a new TextChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.ITextChart);

            /** TextChart body. */
            public body: string;

            /**
             * Creates a new TextChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns TextChart instance
             */
            public static create(properties?: px.vispb.ITextChart): px.vispb.TextChart;

            /**
             * Encodes the specified TextChart message. Does not implicitly {@link px.vispb.TextChart.verify|verify} messages.
             * @param message TextChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.ITextChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified TextChart message, length delimited. Does not implicitly {@link px.vispb.TextChart.verify|verify} messages.
             * @param message TextChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.ITextChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a TextChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns TextChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.TextChart;

            /**
             * Decodes a TextChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns TextChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.TextChart;

            /**
             * Verifies a TextChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a TextChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns TextChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.TextChart;

            /**
             * Creates a plain object from a TextChart message. Also converts values to other types if specified.
             * @param message TextChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.TextChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this TextChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a VegaChart. */
        interface IVegaChart {

            /** VegaChart spec */
            spec?: (string|null);
        }

        /** Represents a VegaChart. */
        class VegaChart implements IVegaChart {

            /**
             * Constructs a new VegaChart.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IVegaChart);

            /** VegaChart spec. */
            public spec: string;

            /**
             * Creates a new VegaChart instance using the specified properties.
             * @param [properties] Properties to set
             * @returns VegaChart instance
             */
            public static create(properties?: px.vispb.IVegaChart): px.vispb.VegaChart;

            /**
             * Encodes the specified VegaChart message. Does not implicitly {@link px.vispb.VegaChart.verify|verify} messages.
             * @param message VegaChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IVegaChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified VegaChart message, length delimited. Does not implicitly {@link px.vispb.VegaChart.verify|verify} messages.
             * @param message VegaChart message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IVegaChart, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a VegaChart message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns VegaChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.VegaChart;

            /**
             * Decodes a VegaChart message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns VegaChart
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.VegaChart;

            /**
             * Verifies a VegaChart message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a VegaChart message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns VegaChart
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.VegaChart;

            /**
             * Creates a plain object from a VegaChart message. Also converts values to other types if specified.
             * @param message VegaChart
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.VegaChart, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this VegaChart to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a Table. */
        interface ITable {

            /** Table gutterColumn */
            gutterColumn?: (string|null);
        }

        /** Represents a Table. */
        class Table implements ITable {

            /**
             * Constructs a new Table.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.ITable);

            /** Table gutterColumn. */
            public gutterColumn: string;

            /**
             * Creates a new Table instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Table instance
             */
            public static create(properties?: px.vispb.ITable): px.vispb.Table;

            /**
             * Encodes the specified Table message. Does not implicitly {@link px.vispb.Table.verify|verify} messages.
             * @param message Table message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.ITable, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Table message, length delimited. Does not implicitly {@link px.vispb.Table.verify|verify} messages.
             * @param message Table message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.ITable, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a Table message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Table
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Table;

            /**
             * Decodes a Table message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Table
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Table;

            /**
             * Verifies a Table message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a Table message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Table
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.Table;

            /**
             * Creates a plain object from a Table message. Also converts values to other types if specified.
             * @param message Table
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.Table, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Table to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a Graph. */
        interface IGraph {

            /** Graph dotColumn */
            dotColumn?: (string|null);

            /** Graph adjacencyList */
            adjacencyList?: (px.vispb.Graph.IAdjacencyList|null);

            /** Graph edgeWeightColumn */
            edgeWeightColumn?: (string|null);

            /** Graph nodeWeightColumn */
            nodeWeightColumn?: (string|null);

            /** Graph edgeColorColumn */
            edgeColorColumn?: (string|null);

            /** Graph edgeThresholds */
            edgeThresholds?: (px.vispb.Graph.IEdgeThresholds|null);

            /** Graph edgeHoverInfo */
            edgeHoverInfo?: (string[]|null);

            /** Graph edgeLength */
            edgeLength?: (number|Long|null);

            /** Graph enableDefaultHierarchy */
            enableDefaultHierarchy?: (boolean|null);
        }

        /** Represents a Graph. */
        class Graph implements IGraph {

            /**
             * Constructs a new Graph.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IGraph);

            /** Graph dotColumn. */
            public dotColumn?: (string|null);

            /** Graph adjacencyList. */
            public adjacencyList?: (px.vispb.Graph.IAdjacencyList|null);

            /** Graph edgeWeightColumn. */
            public edgeWeightColumn: string;

            /** Graph nodeWeightColumn. */
            public nodeWeightColumn: string;

            /** Graph edgeColorColumn. */
            public edgeColorColumn: string;

            /** Graph edgeThresholds. */
            public edgeThresholds?: (px.vispb.Graph.IEdgeThresholds|null);

            /** Graph edgeHoverInfo. */
            public edgeHoverInfo: string[];

            /** Graph edgeLength. */
            public edgeLength: (number|Long);

            /** Graph enableDefaultHierarchy. */
            public enableDefaultHierarchy: boolean;

            /** Graph input. */
            public input?: ("dotColumn"|"adjacencyList");

            /**
             * Creates a new Graph instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Graph instance
             */
            public static create(properties?: px.vispb.IGraph): px.vispb.Graph;

            /**
             * Encodes the specified Graph message. Does not implicitly {@link px.vispb.Graph.verify|verify} messages.
             * @param message Graph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Graph message, length delimited. Does not implicitly {@link px.vispb.Graph.verify|verify} messages.
             * @param message Graph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a Graph message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Graph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Graph;

            /**
             * Decodes a Graph message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Graph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Graph;

            /**
             * Verifies a Graph message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a Graph message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Graph
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.Graph;

            /**
             * Creates a plain object from a Graph message. Also converts values to other types if specified.
             * @param message Graph
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.Graph, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Graph to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        namespace Graph {

            /** Properties of an AdjacencyList. */
            interface IAdjacencyList {

                /** AdjacencyList fromColumn */
                fromColumn?: (string|null);

                /** AdjacencyList toColumn */
                toColumn?: (string|null);
            }

            /** Represents an AdjacencyList. */
            class AdjacencyList implements IAdjacencyList {

                /**
                 * Constructs a new AdjacencyList.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Graph.IAdjacencyList);

                /** AdjacencyList fromColumn. */
                public fromColumn: string;

                /** AdjacencyList toColumn. */
                public toColumn: string;

                /**
                 * Creates a new AdjacencyList instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns AdjacencyList instance
                 */
                public static create(properties?: px.vispb.Graph.IAdjacencyList): px.vispb.Graph.AdjacencyList;

                /**
                 * Encodes the specified AdjacencyList message. Does not implicitly {@link px.vispb.Graph.AdjacencyList.verify|verify} messages.
                 * @param message AdjacencyList message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Graph.IAdjacencyList, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified AdjacencyList message, length delimited. Does not implicitly {@link px.vispb.Graph.AdjacencyList.verify|verify} messages.
                 * @param message AdjacencyList message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Graph.IAdjacencyList, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes an AdjacencyList message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns AdjacencyList
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Graph.AdjacencyList;

                /**
                 * Decodes an AdjacencyList message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns AdjacencyList
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Graph.AdjacencyList;

                /**
                 * Verifies an AdjacencyList message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates an AdjacencyList message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns AdjacencyList
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Graph.AdjacencyList;

                /**
                 * Creates a plain object from an AdjacencyList message. Also converts values to other types if specified.
                 * @param message AdjacencyList
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Graph.AdjacencyList, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this AdjacencyList to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }

            /** Properties of an EdgeThresholds. */
            interface IEdgeThresholds {

                /** EdgeThresholds mediumThreshold */
                mediumThreshold?: (number|Long|null);

                /** EdgeThresholds highThreshold */
                highThreshold?: (number|Long|null);
            }

            /** Represents an EdgeThresholds. */
            class EdgeThresholds implements IEdgeThresholds {

                /**
                 * Constructs a new EdgeThresholds.
                 * @param [properties] Properties to set
                 */
                constructor(properties?: px.vispb.Graph.IEdgeThresholds);

                /** EdgeThresholds mediumThreshold. */
                public mediumThreshold: (number|Long);

                /** EdgeThresholds highThreshold. */
                public highThreshold: (number|Long);

                /**
                 * Creates a new EdgeThresholds instance using the specified properties.
                 * @param [properties] Properties to set
                 * @returns EdgeThresholds instance
                 */
                public static create(properties?: px.vispb.Graph.IEdgeThresholds): px.vispb.Graph.EdgeThresholds;

                /**
                 * Encodes the specified EdgeThresholds message. Does not implicitly {@link px.vispb.Graph.EdgeThresholds.verify|verify} messages.
                 * @param message EdgeThresholds message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encode(message: px.vispb.Graph.IEdgeThresholds, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Encodes the specified EdgeThresholds message, length delimited. Does not implicitly {@link px.vispb.Graph.EdgeThresholds.verify|verify} messages.
                 * @param message EdgeThresholds message or plain object to encode
                 * @param [writer] Writer to encode to
                 * @returns Writer
                 */
                public static encodeDelimited(message: px.vispb.Graph.IEdgeThresholds, writer?: $protobuf.Writer): $protobuf.Writer;

                /**
                 * Decodes an EdgeThresholds message from the specified reader or buffer.
                 * @param reader Reader or buffer to decode from
                 * @param [length] Message length if known beforehand
                 * @returns EdgeThresholds
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.Graph.EdgeThresholds;

                /**
                 * Decodes an EdgeThresholds message from the specified reader or buffer, length delimited.
                 * @param reader Reader or buffer to decode from
                 * @returns EdgeThresholds
                 * @throws {Error} If the payload is not a reader or valid buffer
                 * @throws {$protobuf.util.ProtocolError} If required fields are missing
                 */
                public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.Graph.EdgeThresholds;

                /**
                 * Verifies an EdgeThresholds message.
                 * @param message Plain object to verify
                 * @returns `null` if valid, otherwise the reason why it is not
                 */
                public static verify(message: { [k: string]: any }): (string|null);

                /**
                 * Creates an EdgeThresholds message from a plain object. Also converts values to their respective internal types.
                 * @param object Plain object
                 * @returns EdgeThresholds
                 */
                public static fromObject(object: { [k: string]: any }): px.vispb.Graph.EdgeThresholds;

                /**
                 * Creates a plain object from an EdgeThresholds message. Also converts values to other types if specified.
                 * @param message EdgeThresholds
                 * @param [options] Conversion options
                 * @returns Plain object
                 */
                public static toObject(message: px.vispb.Graph.EdgeThresholds, options?: $protobuf.IConversionOptions): { [k: string]: any };

                /**
                 * Converts this EdgeThresholds to JSON.
                 * @returns JSON object
                 */
                public toJSON(): { [k: string]: any };
            }
        }

        /** Properties of a RequestGraph. */
        interface IRequestGraph {

            /** RequestGraph requestorPodColumn */
            requestorPodColumn?: (string|null);

            /** RequestGraph responderPodColumn */
            responderPodColumn?: (string|null);

            /** RequestGraph requestorServiceColumn */
            requestorServiceColumn?: (string|null);

            /** RequestGraph responderServiceColumn */
            responderServiceColumn?: (string|null);

            /** RequestGraph requestorIPColumn */
            requestorIPColumn?: (string|null);

            /** RequestGraph responderIPColumn */
            responderIPColumn?: (string|null);

            /** RequestGraph p50Column */
            p50Column?: (string|null);

            /** RequestGraph p90Column */
            p90Column?: (string|null);

            /** RequestGraph p99Column */
            p99Column?: (string|null);

            /** RequestGraph errorRateColumn */
            errorRateColumn?: (string|null);

            /** RequestGraph requestsPerSecondColumn */
            requestsPerSecondColumn?: (string|null);

            /** RequestGraph inboundBytesPerSecondColumn */
            inboundBytesPerSecondColumn?: (string|null);

            /** RequestGraph outboundBytesPerSecondColumn */
            outboundBytesPerSecondColumn?: (string|null);

            /** RequestGraph totalRequestCountColumn */
            totalRequestCountColumn?: (string|null);
        }

        /** Represents a RequestGraph. */
        class RequestGraph implements IRequestGraph {

            /**
             * Constructs a new RequestGraph.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IRequestGraph);

            /** RequestGraph requestorPodColumn. */
            public requestorPodColumn: string;

            /** RequestGraph responderPodColumn. */
            public responderPodColumn: string;

            /** RequestGraph requestorServiceColumn. */
            public requestorServiceColumn: string;

            /** RequestGraph responderServiceColumn. */
            public responderServiceColumn: string;

            /** RequestGraph requestorIPColumn. */
            public requestorIPColumn: string;

            /** RequestGraph responderIPColumn. */
            public responderIPColumn: string;

            /** RequestGraph p50Column. */
            public p50Column: string;

            /** RequestGraph p90Column. */
            public p90Column: string;

            /** RequestGraph p99Column. */
            public p99Column: string;

            /** RequestGraph errorRateColumn. */
            public errorRateColumn: string;

            /** RequestGraph requestsPerSecondColumn. */
            public requestsPerSecondColumn: string;

            /** RequestGraph inboundBytesPerSecondColumn. */
            public inboundBytesPerSecondColumn: string;

            /** RequestGraph outboundBytesPerSecondColumn. */
            public outboundBytesPerSecondColumn: string;

            /** RequestGraph totalRequestCountColumn. */
            public totalRequestCountColumn: string;

            /**
             * Creates a new RequestGraph instance using the specified properties.
             * @param [properties] Properties to set
             * @returns RequestGraph instance
             */
            public static create(properties?: px.vispb.IRequestGraph): px.vispb.RequestGraph;

            /**
             * Encodes the specified RequestGraph message. Does not implicitly {@link px.vispb.RequestGraph.verify|verify} messages.
             * @param message RequestGraph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IRequestGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified RequestGraph message, length delimited. Does not implicitly {@link px.vispb.RequestGraph.verify|verify} messages.
             * @param message RequestGraph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IRequestGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a RequestGraph message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns RequestGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.RequestGraph;

            /**
             * Decodes a RequestGraph message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns RequestGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.RequestGraph;

            /**
             * Verifies a RequestGraph message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a RequestGraph message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns RequestGraph
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.RequestGraph;

            /**
             * Creates a plain object from a RequestGraph message. Also converts values to other types if specified.
             * @param message RequestGraph
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.RequestGraph, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this RequestGraph to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a StackTraceFlameGraph. */
        interface IStackTraceFlameGraph {

            /** StackTraceFlameGraph stacktraceColumn */
            stacktraceColumn?: (string|null);

            /** StackTraceFlameGraph countColumn */
            countColumn?: (string|null);

            /** StackTraceFlameGraph percentageColumn */
            percentageColumn?: (string|null);

            /** StackTraceFlameGraph namespaceColumn */
            namespaceColumn?: (string|null);

            /** StackTraceFlameGraph podColumn */
            podColumn?: (string|null);

            /** StackTraceFlameGraph containerColumn */
            containerColumn?: (string|null);

            /** StackTraceFlameGraph pidColumn */
            pidColumn?: (string|null);

            /** StackTraceFlameGraph nodeColumn */
            nodeColumn?: (string|null);

            /** StackTraceFlameGraph percentageLabel */
            percentageLabel?: (string|null);
        }

        /** Represents a StackTraceFlameGraph. */
        class StackTraceFlameGraph implements IStackTraceFlameGraph {

            /**
             * Constructs a new StackTraceFlameGraph.
             * @param [properties] Properties to set
             */
            constructor(properties?: px.vispb.IStackTraceFlameGraph);

            /** StackTraceFlameGraph stacktraceColumn. */
            public stacktraceColumn: string;

            /** StackTraceFlameGraph countColumn. */
            public countColumn: string;

            /** StackTraceFlameGraph percentageColumn. */
            public percentageColumn: string;

            /** StackTraceFlameGraph namespaceColumn. */
            public namespaceColumn: string;

            /** StackTraceFlameGraph podColumn. */
            public podColumn: string;

            /** StackTraceFlameGraph containerColumn. */
            public containerColumn: string;

            /** StackTraceFlameGraph pidColumn. */
            public pidColumn: string;

            /** StackTraceFlameGraph nodeColumn. */
            public nodeColumn: string;

            /** StackTraceFlameGraph percentageLabel. */
            public percentageLabel: string;

            /**
             * Creates a new StackTraceFlameGraph instance using the specified properties.
             * @param [properties] Properties to set
             * @returns StackTraceFlameGraph instance
             */
            public static create(properties?: px.vispb.IStackTraceFlameGraph): px.vispb.StackTraceFlameGraph;

            /**
             * Encodes the specified StackTraceFlameGraph message. Does not implicitly {@link px.vispb.StackTraceFlameGraph.verify|verify} messages.
             * @param message StackTraceFlameGraph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: px.vispb.IStackTraceFlameGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified StackTraceFlameGraph message, length delimited. Does not implicitly {@link px.vispb.StackTraceFlameGraph.verify|verify} messages.
             * @param message StackTraceFlameGraph message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: px.vispb.IStackTraceFlameGraph, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a StackTraceFlameGraph message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns StackTraceFlameGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): px.vispb.StackTraceFlameGraph;

            /**
             * Decodes a StackTraceFlameGraph message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns StackTraceFlameGraph
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): px.vispb.StackTraceFlameGraph;

            /**
             * Verifies a StackTraceFlameGraph message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a StackTraceFlameGraph message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns StackTraceFlameGraph
             */
            public static fromObject(object: { [k: string]: any }): px.vispb.StackTraceFlameGraph;

            /**
             * Creates a plain object from a StackTraceFlameGraph message. Also converts values to other types if specified.
             * @param message StackTraceFlameGraph
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: px.vispb.StackTraceFlameGraph, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this StackTraceFlameGraph to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }
    }
}

/** Namespace google. */
export namespace google {

    /** Namespace protobuf. */
    namespace protobuf {

        /** Properties of an Any. */
        interface IAny {

            /** Any type_url */
            type_url?: (string|null);

            /** Any value */
            value?: (Uint8Array|null);
        }

        /** Represents an Any. */
        class Any implements IAny {

            /**
             * Constructs a new Any.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IAny);

            /** Any type_url. */
            public type_url: string;

            /** Any value. */
            public value: Uint8Array;

            /**
             * Creates a new Any instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Any instance
             */
            public static create(properties?: google.protobuf.IAny): google.protobuf.Any;

            /**
             * Encodes the specified Any message. Does not implicitly {@link google.protobuf.Any.verify|verify} messages.
             * @param message Any message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IAny, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Any message, length delimited. Does not implicitly {@link google.protobuf.Any.verify|verify} messages.
             * @param message Any message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IAny, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes an Any message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Any
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.Any;

            /**
             * Decodes an Any message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Any
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.Any;

            /**
             * Verifies an Any message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates an Any message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Any
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.Any;

            /**
             * Creates a plain object from an Any message. Also converts values to other types if specified.
             * @param message Any
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.Any, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Any to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a DoubleValue. */
        interface IDoubleValue {

            /** DoubleValue value */
            value?: (number|null);
        }

        /** Represents a DoubleValue. */
        class DoubleValue implements IDoubleValue {

            /**
             * Constructs a new DoubleValue.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IDoubleValue);

            /** DoubleValue value. */
            public value: number;

            /**
             * Creates a new DoubleValue instance using the specified properties.
             * @param [properties] Properties to set
             * @returns DoubleValue instance
             */
            public static create(properties?: google.protobuf.IDoubleValue): google.protobuf.DoubleValue;

            /**
             * Encodes the specified DoubleValue message. Does not implicitly {@link google.protobuf.DoubleValue.verify|verify} messages.
             * @param message DoubleValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IDoubleValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified DoubleValue message, length delimited. Does not implicitly {@link google.protobuf.DoubleValue.verify|verify} messages.
             * @param message DoubleValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IDoubleValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a DoubleValue message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns DoubleValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.DoubleValue;

            /**
             * Decodes a DoubleValue message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns DoubleValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.DoubleValue;

            /**
             * Verifies a DoubleValue message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a DoubleValue message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns DoubleValue
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.DoubleValue;

            /**
             * Creates a plain object from a DoubleValue message. Also converts values to other types if specified.
             * @param message DoubleValue
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.DoubleValue, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this DoubleValue to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a FloatValue. */
        interface IFloatValue {

            /** FloatValue value */
            value?: (number|null);
        }

        /** Represents a FloatValue. */
        class FloatValue implements IFloatValue {

            /**
             * Constructs a new FloatValue.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IFloatValue);

            /** FloatValue value. */
            public value: number;

            /**
             * Creates a new FloatValue instance using the specified properties.
             * @param [properties] Properties to set
             * @returns FloatValue instance
             */
            public static create(properties?: google.protobuf.IFloatValue): google.protobuf.FloatValue;

            /**
             * Encodes the specified FloatValue message. Does not implicitly {@link google.protobuf.FloatValue.verify|verify} messages.
             * @param message FloatValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IFloatValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified FloatValue message, length delimited. Does not implicitly {@link google.protobuf.FloatValue.verify|verify} messages.
             * @param message FloatValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IFloatValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a FloatValue message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns FloatValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.FloatValue;

            /**
             * Decodes a FloatValue message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns FloatValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.FloatValue;

            /**
             * Verifies a FloatValue message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a FloatValue message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns FloatValue
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.FloatValue;

            /**
             * Creates a plain object from a FloatValue message. Also converts values to other types if specified.
             * @param message FloatValue
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.FloatValue, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this FloatValue to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of an Int64Value. */
        interface IInt64Value {

            /** Int64Value value */
            value?: (number|Long|null);
        }

        /** Represents an Int64Value. */
        class Int64Value implements IInt64Value {

            /**
             * Constructs a new Int64Value.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IInt64Value);

            /** Int64Value value. */
            public value: (number|Long);

            /**
             * Creates a new Int64Value instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Int64Value instance
             */
            public static create(properties?: google.protobuf.IInt64Value): google.protobuf.Int64Value;

            /**
             * Encodes the specified Int64Value message. Does not implicitly {@link google.protobuf.Int64Value.verify|verify} messages.
             * @param message Int64Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IInt64Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Int64Value message, length delimited. Does not implicitly {@link google.protobuf.Int64Value.verify|verify} messages.
             * @param message Int64Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IInt64Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes an Int64Value message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Int64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.Int64Value;

            /**
             * Decodes an Int64Value message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Int64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.Int64Value;

            /**
             * Verifies an Int64Value message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates an Int64Value message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Int64Value
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.Int64Value;

            /**
             * Creates a plain object from an Int64Value message. Also converts values to other types if specified.
             * @param message Int64Value
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.Int64Value, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Int64Value to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a UInt64Value. */
        interface IUInt64Value {

            /** UInt64Value value */
            value?: (number|Long|null);
        }

        /** Represents a UInt64Value. */
        class UInt64Value implements IUInt64Value {

            /**
             * Constructs a new UInt64Value.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IUInt64Value);

            /** UInt64Value value. */
            public value: (number|Long);

            /**
             * Creates a new UInt64Value instance using the specified properties.
             * @param [properties] Properties to set
             * @returns UInt64Value instance
             */
            public static create(properties?: google.protobuf.IUInt64Value): google.protobuf.UInt64Value;

            /**
             * Encodes the specified UInt64Value message. Does not implicitly {@link google.protobuf.UInt64Value.verify|verify} messages.
             * @param message UInt64Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IUInt64Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified UInt64Value message, length delimited. Does not implicitly {@link google.protobuf.UInt64Value.verify|verify} messages.
             * @param message UInt64Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IUInt64Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a UInt64Value message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns UInt64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.UInt64Value;

            /**
             * Decodes a UInt64Value message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns UInt64Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.UInt64Value;

            /**
             * Verifies a UInt64Value message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a UInt64Value message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns UInt64Value
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.UInt64Value;

            /**
             * Creates a plain object from a UInt64Value message. Also converts values to other types if specified.
             * @param message UInt64Value
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.UInt64Value, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this UInt64Value to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of an Int32Value. */
        interface IInt32Value {

            /** Int32Value value */
            value?: (number|null);
        }

        /** Represents an Int32Value. */
        class Int32Value implements IInt32Value {

            /**
             * Constructs a new Int32Value.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IInt32Value);

            /** Int32Value value. */
            public value: number;

            /**
             * Creates a new Int32Value instance using the specified properties.
             * @param [properties] Properties to set
             * @returns Int32Value instance
             */
            public static create(properties?: google.protobuf.IInt32Value): google.protobuf.Int32Value;

            /**
             * Encodes the specified Int32Value message. Does not implicitly {@link google.protobuf.Int32Value.verify|verify} messages.
             * @param message Int32Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IInt32Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified Int32Value message, length delimited. Does not implicitly {@link google.protobuf.Int32Value.verify|verify} messages.
             * @param message Int32Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IInt32Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes an Int32Value message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns Int32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.Int32Value;

            /**
             * Decodes an Int32Value message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns Int32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.Int32Value;

            /**
             * Verifies an Int32Value message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates an Int32Value message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns Int32Value
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.Int32Value;

            /**
             * Creates a plain object from an Int32Value message. Also converts values to other types if specified.
             * @param message Int32Value
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.Int32Value, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this Int32Value to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a UInt32Value. */
        interface IUInt32Value {

            /** UInt32Value value */
            value?: (number|null);
        }

        /** Represents a UInt32Value. */
        class UInt32Value implements IUInt32Value {

            /**
             * Constructs a new UInt32Value.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IUInt32Value);

            /** UInt32Value value. */
            public value: number;

            /**
             * Creates a new UInt32Value instance using the specified properties.
             * @param [properties] Properties to set
             * @returns UInt32Value instance
             */
            public static create(properties?: google.protobuf.IUInt32Value): google.protobuf.UInt32Value;

            /**
             * Encodes the specified UInt32Value message. Does not implicitly {@link google.protobuf.UInt32Value.verify|verify} messages.
             * @param message UInt32Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IUInt32Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified UInt32Value message, length delimited. Does not implicitly {@link google.protobuf.UInt32Value.verify|verify} messages.
             * @param message UInt32Value message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IUInt32Value, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a UInt32Value message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns UInt32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.UInt32Value;

            /**
             * Decodes a UInt32Value message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns UInt32Value
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.UInt32Value;

            /**
             * Verifies a UInt32Value message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a UInt32Value message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns UInt32Value
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.UInt32Value;

            /**
             * Creates a plain object from a UInt32Value message. Also converts values to other types if specified.
             * @param message UInt32Value
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.UInt32Value, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this UInt32Value to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a BoolValue. */
        interface IBoolValue {

            /** BoolValue value */
            value?: (boolean|null);
        }

        /** Represents a BoolValue. */
        class BoolValue implements IBoolValue {

            /**
             * Constructs a new BoolValue.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IBoolValue);

            /** BoolValue value. */
            public value: boolean;

            /**
             * Creates a new BoolValue instance using the specified properties.
             * @param [properties] Properties to set
             * @returns BoolValue instance
             */
            public static create(properties?: google.protobuf.IBoolValue): google.protobuf.BoolValue;

            /**
             * Encodes the specified BoolValue message. Does not implicitly {@link google.protobuf.BoolValue.verify|verify} messages.
             * @param message BoolValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IBoolValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified BoolValue message, length delimited. Does not implicitly {@link google.protobuf.BoolValue.verify|verify} messages.
             * @param message BoolValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IBoolValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a BoolValue message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns BoolValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.BoolValue;

            /**
             * Decodes a BoolValue message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns BoolValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.BoolValue;

            /**
             * Verifies a BoolValue message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a BoolValue message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns BoolValue
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.BoolValue;

            /**
             * Creates a plain object from a BoolValue message. Also converts values to other types if specified.
             * @param message BoolValue
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.BoolValue, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this BoolValue to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a StringValue. */
        interface IStringValue {

            /** StringValue value */
            value?: (string|null);
        }

        /** Represents a StringValue. */
        class StringValue implements IStringValue {

            /**
             * Constructs a new StringValue.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IStringValue);

            /** StringValue value. */
            public value: string;

            /**
             * Creates a new StringValue instance using the specified properties.
             * @param [properties] Properties to set
             * @returns StringValue instance
             */
            public static create(properties?: google.protobuf.IStringValue): google.protobuf.StringValue;

            /**
             * Encodes the specified StringValue message. Does not implicitly {@link google.protobuf.StringValue.verify|verify} messages.
             * @param message StringValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IStringValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified StringValue message, length delimited. Does not implicitly {@link google.protobuf.StringValue.verify|verify} messages.
             * @param message StringValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IStringValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a StringValue message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns StringValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.StringValue;

            /**
             * Decodes a StringValue message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns StringValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.StringValue;

            /**
             * Verifies a StringValue message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a StringValue message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns StringValue
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.StringValue;

            /**
             * Creates a plain object from a StringValue message. Also converts values to other types if specified.
             * @param message StringValue
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.StringValue, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this StringValue to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }

        /** Properties of a BytesValue. */
        interface IBytesValue {

            /** BytesValue value */
            value?: (Uint8Array|null);
        }

        /** Represents a BytesValue. */
        class BytesValue implements IBytesValue {

            /**
             * Constructs a new BytesValue.
             * @param [properties] Properties to set
             */
            constructor(properties?: google.protobuf.IBytesValue);

            /** BytesValue value. */
            public value: Uint8Array;

            /**
             * Creates a new BytesValue instance using the specified properties.
             * @param [properties] Properties to set
             * @returns BytesValue instance
             */
            public static create(properties?: google.protobuf.IBytesValue): google.protobuf.BytesValue;

            /**
             * Encodes the specified BytesValue message. Does not implicitly {@link google.protobuf.BytesValue.verify|verify} messages.
             * @param message BytesValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encode(message: google.protobuf.IBytesValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Encodes the specified BytesValue message, length delimited. Does not implicitly {@link google.protobuf.BytesValue.verify|verify} messages.
             * @param message BytesValue message or plain object to encode
             * @param [writer] Writer to encode to
             * @returns Writer
             */
            public static encodeDelimited(message: google.protobuf.IBytesValue, writer?: $protobuf.Writer): $protobuf.Writer;

            /**
             * Decodes a BytesValue message from the specified reader or buffer.
             * @param reader Reader or buffer to decode from
             * @param [length] Message length if known beforehand
             * @returns BytesValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decode(reader: ($protobuf.Reader|Uint8Array), length?: number): google.protobuf.BytesValue;

            /**
             * Decodes a BytesValue message from the specified reader or buffer, length delimited.
             * @param reader Reader or buffer to decode from
             * @returns BytesValue
             * @throws {Error} If the payload is not a reader or valid buffer
             * @throws {$protobuf.util.ProtocolError} If required fields are missing
             */
            public static decodeDelimited(reader: ($protobuf.Reader|Uint8Array)): google.protobuf.BytesValue;

            /**
             * Verifies a BytesValue message.
             * @param message Plain object to verify
             * @returns `null` if valid, otherwise the reason why it is not
             */
            public static verify(message: { [k: string]: any }): (string|null);

            /**
             * Creates a BytesValue message from a plain object. Also converts values to their respective internal types.
             * @param object Plain object
             * @returns BytesValue
             */
            public static fromObject(object: { [k: string]: any }): google.protobuf.BytesValue;

            /**
             * Creates a plain object from a BytesValue message. Also converts values to other types if specified.
             * @param message BytesValue
             * @param [options] Conversion options
             * @returns Plain object
             */
            public static toObject(message: google.protobuf.BytesValue, options?: $protobuf.IConversionOptions): { [k: string]: any };

            /**
             * Converts this BytesValue to JSON.
             * @returns JSON object
             */
            public toJSON(): { [k: string]: any };
        }
    }
}
