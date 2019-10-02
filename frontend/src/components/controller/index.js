import React from 'react'
import { SketchPicker } from 'react-color'
import Select from 'react-select'
import { connect } from 'mqtt'
import { ToastContainer, toast } from 'react-toastify'
import Slider from 'rc-slider'
import Tooltip from 'rc-tooltip'
import 'rc-slider/assets/index.css'
import 'react-toastify/dist/ReactToastify.min.css'
import './style.scss'

const selectOptions = [
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
  { value: 'm', label: 'Music' },
]

const mqttControlTopic = 'rgb/control'
const mqttErrorTopic = 'rgb/error'

const mqttOptions = {
  port: process.env.GATSBY_MQTT_PORT,
  host: process.env.GATSBY_MQTT_HOST,
  clientId:
    'mqttjs_' +
    Math.random()
      .toString(16)
      .substr(2, 8),
  username: process.env.GATSBY_MQTT_USERNAME,
  password: process.env.GATSBY_MQTT_PASSWORD,
  keepalive: 60,
  reconnectPeriod: 1000,
  clean: true,
  encoding: 'utf8',
}

const maxSpeed = 20
const defaultSpeed = 1
const maxPulse = 5
const defaultPulse = 0
const maxBrightness = 255
const defaultBrightness = 255
const stepSize = 0.1
const Handle = Slider.Handle

const handle = props => {
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

const defaultColor = {
  r: 0,
  g: 255,
  b: 233,
  a: 1,
}

class Controller extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      on: false,
      mode: null,
      color: Object.assign({}, defaultColor),
      speed: defaultSpeed,
      pulse: defaultPulse,
      password: '',
      formErrors: { password: '' },
      passwordValid: false,
      formValid: false,
      client: null,
      brightness: 255,
    }
  }

  componentDidMount() {
    this.state.client = connect(
      process.env.GATSBY_MQTT_HOST,
      mqttOptions
    )
    this.state.client.on('disconnect', () => {
      console.log('reconnect')
      this.state.client.reconnect()
    })
    this.state.client.on('connect', () => {
      console.log('connected!')
      this.state.client.subscribe(mqttErrorTopic, err => {
        if (!err) console.log('subscribed to error topic')
        else toast.error(`error connecting: ${JSON.stringify(err)}`)
      })
    })
    this.state.client.on('message', (topic, message) => {
      // message is Buffer
      const messageStr = message.toString()
      console.log(`${topic}: ${messageStr}`)
      if (topic === mqttErrorTopic) {
        const messageObj = JSON.parse(messageStr)
        if (messageObj && messageObj.error) toast.error(messageStr)
        else toast.error('error: no error message found')
      } else {
        toast.info(messageStr)
      }
    })
  }

  handleColorChange = color => {
    this.setState({ color: color.rgb })
  }

  handleUserInput = e => {
    const name = e.target.name
    const value = e.target.value
    this.setState({ [name]: value }, () => this.validateField(name, value))
  }

  validateField(fieldName, value) {
    let fieldValidationErrors = this.state.formErrors
    let passwordValid = this.state.passwordValid
    switch (fieldName) {
      case 'password':
        passwordValid = value.length >= 6
        fieldValidationErrors.password = passwordValid
          ? ''
          : 'password is too short'
        break
      default:
        break
    }
    this.setState(
      { formErrors: fieldValidationErrors, passwordValid: passwordValid },
      this.validateForm
    )
  }

  validateForm() {
    this.setState({
      formValid:
        this.state.passwordValid && (!this.state.on || this.state.mode),
    })
  }

  setPower = e => {
    const on = e.currentTarget.value === 'on'
    this.setState({
      on: on,
      mode: null,
      formValid: this.state.passwordValid && (!on || this.state.mode),
    })
  }

  handleModeSelect = selectedOption => {
    this.setState({
      mode: selectedOption.value,
      formValid: this.state.passwordValid,
    })
    console.log(`Option selected:`, selectedOption.value)
  }

  handleSpeedChange = speed => {
    this.setState({ speed })
  }

  handlePulseChange = pulse => {
    this.setState({ pulse })
  }

  handleBrightnessChange = brightness => {
    this.setState({ brightness })
  }

  paramsSelect() {
    if (this.state.mode) {
      if (this.state.mode === selectOptions[0].value) {
        return (
          <div>
            <div className="mt-4">
              <SketchPicker
                color={this.state.color}
                onChangeComplete={this.handleColorChange}
              />
            </div>
            <div className="mt-4">
              <p>Pulse period (s)</p>
              <Slider
                min={0}
                max={maxPulse}
                defaultValue={defaultPulse}
                step={stepSize}
                handle={handle}
                onAfterChange={this.handlePulseChange}
              />
            </div>
          </div>
        )
      } else {
        const speedDiv = (
          <div className="mt-4">
            <p>Speed</p>
            <Slider
              min={0}
              max={maxSpeed}
              defaultValue={defaultSpeed}
              handle={handle}
              onAfterChange={this.handleSpeedChange}
            />
          </div>
        )
        const brightnessDiv = (
          <div className="mt-4">
            <p>Brightness</p>
            <Slider
              min={0}
              max={maxBrightness}
              defaultValue={defaultBrightness}
              handle={handle}
              onAfterChange={this.handleBrightnessChange}
            />
          </div>
        )
        if (this.state.mode === selectOptions[1].value) return brightnessDiv
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

  modeSelect() {
    if (this.state.on) {
      return (
        <div>
          <Select
            value={this.mode}
            onChange={this.handleModeSelect}
            options={selectOptions}
          />
          {this.paramsSelect()}
        </div>
      )
    } else {
      return <div></div>
    }
  }

  onSubmit = evt => {
    evt.preventDefault()
    console.log(this.state.color.a)
    const command = JSON.stringify({
      o: this.state.on,
      m: this.state.mode,
      c: {
        r: this.state.color.r,
        g: this.state.color.g,
        b: this.state.color.b,
        a: Math.round((1 - this.state.color.a) * 255),
      },
      p: this.state.password,
      s: this.state.speed,
      f: this.state.pulse,
      b: this.state.brightness,
    })
    console.log(`send ${command}`)
    this.state.client.publish(mqttControlTopic, command, {}, err => {
      if (err) toast.error(`got error submitting: ${JSON.stringify(err)}`)
    })
  }

  render() {
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
          pauseOnVisibilityChange
          draggable
          pauseOnHover
        />
        <h2>Controller</h2>
        <form onSubmit={this.onSubmit}>
          <div className="form-group mt-4">
            <label htmlFor="powerOn">Power</label>
            <div className="form-check">
              <input
                className="form-check-input"
                type="radio"
                name="powerOn"
                id="powerOn"
                value="on"
                checked={this.state.on}
                onChange={this.setPower}
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
                checked={!this.state.on}
                onChange={this.setPower}
              />
              <label className="form-check-label" htmlFor="powerOff">
                Off
              </label>
            </div>
          </div>
          {this.modeSelect()}
          <div className="form-group mt-4">
            <label htmlFor="password">Password</label>
            <input
              type="password"
              className="form-control"
              name="password"
              id="password"
              placeholder="Password"
              onChange={this.handleUserInput}
            />
          </div>
          <button
            type="submit"
            className="btn btn-primary"
            disabled={!this.state.formValid}
          >
            Submit
          </button>
        </form>
      </div>
    )
  }
}

export default Controller
