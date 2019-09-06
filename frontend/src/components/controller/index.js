import React from 'react'
import { SketchPicker } from 'react-color'
import Select from 'react-select'
import { connect, createClient } from 'mqtt'
import { ToastContainer, toast } from 'react-toastify'
import 'react-toastify/dist/ReactToastify.min.css'
import './style.scss'

const selectOptions = [
  { value: 'color', label: 'Color' },
  { value: 'random', label: 'Random' },
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

class Controller extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      on: false,
      mode: null,
      color: {
        r: 129,
        g: 71,
        b: 196,
        a: 73,
      },
      password: '',
      formErrors: { password: '' },
      passwordValid: false,
      formValid: false,
      client: null,
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

  colorSelect() {
    if (this.state.mode === selectOptions[0].value) {
      return (
        <div className="mt-4">
          <SketchPicker
            color={this.state.color}
            onChangeComplete={this.handleColorChange}
          />
        </div>
      )
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
          {this.colorSelect()}
        </div>
      )
    } else {
      return <div></div>
    }
  }

  onSubmit = evt => {
    evt.preventDefault()
    const command = JSON.stringify({
      on: this.state.on,
      mode: this.state.mode,
      color: {
        r: this.state.color.r,
        g: this.state.color.g,
        b: this.state.color.b,
        a: Math.round((1 - this.state.color.a) * 255),
      },
      password: this.state.password,
    })
    console.log(`send ${command}`)
    this.state.client.publish(mqttControlTopic, command, {}, err => {
      if (err) toast.error(`got error submitting: ${JSON.stringify(err)}`)
      else toast.success('sent command')
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
