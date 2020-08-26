import React from 'react'
import { SketchPicker, RGBColor } from 'react-color'
import Select from 'react-select'
import { connect, MqttClient } from 'mqtt'
import { ToastContainer, toast } from 'react-toastify'
import Slider from 'rc-slider'
import Tooltip from 'rc-tooltip'
import 'rc-slider/assets/index.css'
import 'react-toastify/dist/ReactToastify.min.css'
import './style.scss'
import { useEffect } from 'react'
import { useState } from 'react'

interface SelectOptionType {
  value: string;
  label: string;
}

const selectOptions: SelectOptionType[] = [
  { value: 'c', label: 'Color' },
  { value: 'p', label: 'Periodic' },
  { value: 'w', label: 'Warm colors' },
  { value: 'r', label: 'Rainbow' },
  { value: 'rs', label: 'Rainbow Stripes' },
  { value: 'rsb', label: 'Rainbow Stripes Blend' },
  { value: 'pg', label: 'Purple Green' },
  { value: 'ra', label: 'Random' },
  { value: 'bw', label: 'Black White' },
  { value: 'bwb', label: 'Black White Blend' },
  { value: 'cl', label: 'Cloud' },
  { value: 'pa', label: 'Party' },
  { value: 'a', label: "'murica" },
  { value: 'ab', label: "'murica 2.0" },
  { value: 'ha', label: 'Halloween' },
  { value: 'th', label: 'Thanksgiving' },
  { value: 'ch', label: 'Christmas' },
  { value: 'ny', label: "New Year's" },
  { value: 'ea', label: 'Easter' },
]

const maxSpeed = 20
const defaultSpeed = 1
const maxPulse = 5
const defaultPulse = 0
const maxBrightness = 255
const defaultBrightness = 255
const stepSize = 0.1
const Handle = Slider.Handle

const handle = (props: Record<string, any>) => {
  const { value, dragging, index, ...restProps } = props
  return (
    <Tooltip
      prefixCls="rc-slider-tooltip"
      overlay={value}
      visible={dragging}
      placement="top"
      key={index}
    >
      <Handle value={value} {...restProps} />
    </Tooltip>
  )
}

const defaultColor: RGBColor = {
  r: 255,
  g: 255,
  b: 255,
  a: 1,
}

const defaultMode = selectOptions[0]

const Controller = () => {
  const [on, setOn] = useState(false)
  const [mode, setMode] = useState<SelectOptionType | null>(defaultMode)
  const [color, setColor] = useState<RGBColor>({
    ...defaultColor,
  })
  const [speed, setSpeed] = useState(defaultSpeed)
  const [pulse, setPulse] = useState(defaultPulse)
  const [password, setPassword] = useState('')
  const [passwordValid, setPasswordValid] = useState(false)
  const [formValid, setFormValid] = useState(false)
  const [client, setClient] = useState<MqttClient | null>(null)
  const [brightness, setBrightness] = useState(255)
  useEffect(() => {
    const newClient = connect(process.env.GATSBY_MQTT_HOST, {
      port: parseInt(process.env.GATSBY_MQTT_PORT as string),
      host: process.env.GATSBY_MQTT_HOST as string,
      clientId: 'client_' + Math.random().toString(16).substr(2, 8),
      username: process.env.GATSBY_MQTT_USERNAME,
      password: process.env.GATSBY_MQTT_PASSWORD,
      keepalive: 60,
      reconnectPeriod: 1000,
      clean: true,
    })
    setClient(newClient)
    newClient.on('disconnect', () => {
      console.log('reconnect')
      if (client) client.reconnect()
    })
    newClient.on('connect', () => {
      console.log('connected!')
      newClient.subscribe(
        process.env.GATSBY_MQTT_MESSAGE_TOPIC as string,
        (err) => {
          if (!err) console.log('subscribed to message topic')
          else toast.error(`message connecting: ${JSON.stringify(err)}`)
        }
      )
    })
    newClient.on('message', (topic, message) => {
      // message is Buffer
      const messageStr = message.toString()
      console.log(`${topic}: ${messageStr}`)
      if (topic === process.env.GATSBY_MQTT_MESSAGE_TOPIC) {
        const messageObj = JSON.parse(messageStr)
        if (messageObj) {
          if (messageObj.error) {
            toast.error(messageObj.error);
          } else if (messageObj.info) {
            toast.info(messageObj.info);
          } else {
            toast.error('no message key found')
          }
        } else {
          toast.error('no message object found')
        }
      } else {
        toast.info(messageStr)
      }
    })
  }, [])

  const setPower = (nowOn: boolean) => {
    setOn(nowOn)
    setFormValid(passwordValid && !!(!nowOn || mode))
    setMode(null)
  }

  const paramsSelect = () => {
    if (mode) {
      if (mode.value === selectOptions[0].value) {
        return (
          <div>
            <div className="mt-4">
              <SketchPicker
                color={color}
                onChangeComplete={(colorData) => {
                  setColor(colorData.rgb)
                }}
              />
            </div>
            <div className="mt-4">
              <p>Pulse period (s)</p>
              {/* @ts-ignore */}
              <Slider
                min={0}
                max={maxPulse}
                defaultValue={defaultPulse}
                step={stepSize}
                handle={handle}
                onAfterChange={(val: number) => {
                  setPulse(val)
                }}
              />
            </div>
          </div>
        )
      } else {
        const speedDiv = (
          <div className="mt-4">
            <p>Speed</p>
            {/* @ts-ignore */}
            <Slider
              min={0}
              max={maxSpeed}
              defaultValue={defaultSpeed}
              handle={handle}
              onAfterChange={(val: number) => {
                setSpeed(val)
              }}
            />
          </div>
        )
        const brightnessDiv = (
          <div className="mt-4">
            <p>Brightness</p>
            {/* @ts-ignore */}
            <Slider
              min={0}
              max={maxBrightness}
              defaultValue={defaultBrightness}
              handle={handle}
              onAfterChange={(val: number) => {
                setBrightness(val)
              }}
            />
          </div>
        )
        if (mode.value === selectOptions[1].value) return brightnessDiv
        else
          return (
            <div>
              {speedDiv}
              {brightnessDiv}
            </div>
          )
      }
    } else {
      return <div></div>
    }
  }

  const modeSelect = () => {
    if (on) {
      return (
        <div>
          <Select
            // @ts-ignore
            value={mode}
            onChange={(selectedOption) => {
              if (!selectedOption) return
              const selected = selectedOption as SelectOptionType;
              setMode(selected);
              setFormValid(passwordValid);
              console.log(`Option selected: ${selected.value}`);
            }}
            options={selectOptions}
          />
          {paramsSelect()}
        </div>
      )
    } else {
      return <div></div>
    }
  }

  return (
    <div
      className="container mt-4"
      style={{
        marginBottom: '30rem',
      }}
    >
      <ToastContainer
        position="top-right"
        autoClose={5000}
        hideProgressBar={false}
        newestOnTop={false}
        closeOnClick
        rtl={false}
        draggable
        pauseOnHover
      />
      <h2>Controller</h2>
      <form
        onSubmit={(evt) => {
          evt.preventDefault()
          if (!color.a || !mode) return
          const command = JSON.stringify({
            o: on,
            m: mode.value,
            c: {
              r: color.r,
              g: color.g,
              b: color.b,
              a: Math.round((1 - color.a) * 255),
            },
            p: password,
            s: speed,
            f: pulse,
            b: brightness,
          })
          if (!client) return
          console.log(`send ${command}`)
          client.publish(
            process.env.GATSBY_MQTT_CONTROL_TOPIC as string,
            command,
            {},
            (err) => {
              if (err)
                toast.error(`got error submitting: ${JSON.stringify(err)}`)
            }
          )
        }}
      >
        <div className="form-group mt-4">
          <label htmlFor="powerOn">Power</label>
          <div className="form-check">
            <input
              className="form-check-input"
              type="radio"
              name="powerOn"
              id="powerOn"
              value="on"
              checked={on}
              onChange={() => setPower(true)}
            />
            <label className="form-check-label" htmlFor="powerOn">
              On
            </label>
          </div>
          <div className="form-check">
            <input
              className="form-check-input"
              type="radio"
              name="powerOff"
              id="powerOff"
              value="off"
              checked={!on}
              onChange={() => setPower(true)}
            />
            <label className="form-check-label" htmlFor="powerOff">
              Off
            </label>
          </div>
        </div>
        {modeSelect()}
        <div className="form-group mt-4">
          <label htmlFor="password">Password</label>
          <input
            type="password"
            className="form-control"
            name="password"
            id="password"
            placeholder="Password"
            onChange={(evt) => {
              const password = evt.target.value
              setPassword(password)
              const nowPasswordValid = password.length >= 6
              setPasswordValid(nowPasswordValid)
              setFormValid(nowPasswordValid && !!(!on || mode))
            }}
          />
        </div>
        <button type="submit" className="btn btn-primary" disabled={!formValid}>
          Submit
        </button>
      </form>
    </div>
  )
}

export default Controller
