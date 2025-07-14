import React, { useEffect, useState } from 'react'
// import './App.css'
import 'odin-react/dist/index.css'

import 'bootstrap/dist/css/bootstrap.min.css';

import { OdinApp, TitleCard, DropdownSelector, ToggleSwitch } from 'odin-react';
import { WithEndpoint, useAdapterEndpoint } from 'odin-react';

import Row from 'react-bootstrap/Row';
import Col from 'react-bootstrap/Col';
import Button from 'react-bootstrap/Button';
import Alert from 'react-bootstrap/Alert';
import Dropdown from 'react-bootstrap/Dropdown';
import Form from 'react-bootstrap/Form';
import InputGroup from 'react-bootstrap/InputGroup';
import Stack from 'react-bootstrap/Stack';
import Card from 'react-bootstrap/Card';

const EndpointButton = WithEndpoint(Button);
const EndpointInput = WithEndpoint(Form.Control);
const EndpointDropdown = WithEndpoint(DropdownSelector);
const EndpointToggle = WithEndpoint(ToggleSwitch);


function App() {
  
  const histogramEndpoint = useAdapterEndpoint('hexitec_adapter', import.meta.env.VITE_ENDPOINT_URL, 1000);

  const run_disable = histogramEndpoint.data?.control ? (histogramEndpoint.data.control.running_flag || histogramEndpoint.data.control.status == "disconnected")  : true;

  const postConnectMethod = () => {
    const getPath = "control";
    histogramEndpoint.get(getPath)
    .then((response) => {
      histogramEndpoint.mergeData(response, getPath);
    })
  }


  return (
    <OdinApp title="Hexitec Histogrammer"
    navLinks={["Main"]}
    icon_src="odin.png"
    icon_hover_src="prodin.png">
      <>
      <Row>
        <Col>
          <TitleCard title="Connection">
            <Row>
            <Col>
              <Alert variant={histogramEndpoint.data?.control?.status == "disconnected" ? "danger" : "primary"}>
                {histogramEndpoint.data?.control?.status || "disconnected"}
              </Alert>
            </Col>
            <Col>
              <EndpointButton endpoint={histogramEndpoint} event_type="click" fullpath="connect/connect" value={true}
                              post_method={postConnectMethod} disabled={histogramEndpoint.data?.control?.status ? histogramEndpoint.data?.control.status != "disconnected" : true}>
                Connect
              </EndpointButton>
            </Col>
            </Row>
            <Row>
              <InputGroup>
              <InputGroup.Text>Bus Num:</InputGroup.Text>
              <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="connect/busNum" type="number" disabled={run_disable}/>
              <InputGroup.Text>Device Num:</InputGroup.Text>
              <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="connect/devNum" type="number" disabled={run_disable}/>
              <InputGroup.Text>Func Num:</InputGroup.Text>
              <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="connect/funcNum" type="number" disabled={run_disable}/>
              </InputGroup>
            </Row>
          </TitleCard>
          <TitleCard title="Histogram Format">
            <Stack direction='horizontal'>
            <InputGroup>
              <InputGroup.Text>Number Bins</InputGroup.Text>
              <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/hist_format/bins/selected"
                buttonText={histogramEndpoint.data?.config?.hist_format?.bins?.selected || "Unknown"} disabled={run_disable}>
                  {histogramEndpoint.data?.config?.hist_format?.bins?.options ? histogramEndpoint.data.config.hist_format.bins.options.map(
                    (selection, index) => (
                      <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                    )) : <></>
                  }
                </EndpointDropdown>
            </InputGroup>
            <InputGroup>
              <InputGroup.Text>Run Mode</InputGroup.Text>
              <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/hist_format/run_mode/selected"
                buttonText={histogramEndpoint.data?.config?.hist_format?.run_mode?.selected || "Unknown"} disabled={run_disable}>
                  {histogramEndpoint.data?.config?.hist_format?.bins?.options ? histogramEndpoint.data.config.hist_format.run_mode.options.map(
                    (selection, index) => (
                      <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                    )) : <></>
                  }
                </EndpointDropdown>
            </InputGroup>
            </Stack>
          </TitleCard>
        </Col>
        <Col>
        <TitleCard title="Run">
          <Row>
          <Col>
            <Row>
            <Col>
              <InputGroup>
                <EndpointButton endpoint={histogramEndpoint} event_type="click" fullpath="control/prepare_run" value={true}
                disabled={run_disable}>
                Prepare Run
                </EndpointButton>
              </InputGroup>
            </Col>
            <Col>
              <InputGroup>
                <EndpointButton endpoint={histogramEndpoint} event_type="click" fullpath="control/start_run" value={true}
                disabled={histogramEndpoint.data?.control?.status == "disconnected" || histogramEndpoint.data?.control?.status == "running"}
                variant="success">
                Start Run
                </EndpointButton>  
                <EndpointButton endpoint={histogramEndpoint} event_type="click" fullpath="control/stop_run" value={true}
                disabled={histogramEndpoint.data?.control?.status != "running"}
                variant="danger">
                Stop Run
                </EndpointButton>
              </InputGroup>
            </Col>
            </Row>
          </Col>
          <Col>
            <InputGroup>
              <InputGroup.Text>Run Timer</InputGroup.Text>
              <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="control/run_timer" type="number"/>
            </InputGroup>
          </Col>
          </Row>
          <Row>
            <Col>
              <Card>
              <Card.Body>
              <Stack direction='horizontal'>
                <InputGroup>
                  <InputGroup.Text>Input Frames</InputGroup.Text>
                  <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/itfg/input_frames" type="number"/>
                </InputGroup>
                <InputGroup>
                  <InputGroup.Text>Output Frames</InputGroup.Text>
                  <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/itfg/output_frames" type="number"/>
                </InputGroup>
                <InputGroup>
                  <InputGroup.Text>ITfg Cycles</InputGroup.Text>
                  <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/itfg/cycles" type="number"/>
                </InputGroup>
              </Stack>
              </Card.Body>
              </Card>
            </Col>
            </Row>
            <hr />
            <Row>
            <Col>
              <Card>
                <Card.Body>
                  <Stack>
                    <InputGroup>
                      <InputGroup.Text>Detector Frames</InputGroup.Text>
                      <Form.Control disabled value={histogramEndpoint.data?.control?.frames?.detector_frames || 0}/>
                      <InputGroup.Text>Total Raw Hits         </InputGroup.Text>
                      <Form.Control disabled value={histogramEndpoint.data?.control?.frames?.raw_hits || 0}/>
                    </InputGroup>
                    <InputGroup>
                      <InputGroup.Text>UDP Frames</InputGroup.Text>
                      <Form.Control disabled value={histogramEndpoint.data?.control?.frames?.udp_frames || 0}/>
                      <InputGroup.Text>Completed Time Frames</InputGroup.Text>
                      <Form.Control disabled value={histogramEndpoint.data?.control?.frames?.complete_time_frames || 0}/>
                    </InputGroup>
                  </Stack>
                </Card.Body>
              </Card>
            </Col>
            </Row>
        </TitleCard>
        </Col>
      </Row>
      <Row>
        <Col>
        <TitleCard title="Threshholds">
          <Stack gap={1}>
          <InputGroup>
          <InputGroup.Text>Main Threshold</InputGroup.Text>
          <InputGroup.Text>Positive</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/main/pos" type="number"/>
          <InputGroup.Text>Negative</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/main/neg" type="number"/>
          </InputGroup>
          <InputGroup>
          <InputGroup.Text>Low Threshold</InputGroup.Text>
          <InputGroup.Text>Positive</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/low/pos" type="number"/>
          <InputGroup.Text>Negative</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/low/neg" type="number"/>
          </InputGroup>
          <InputGroup>
          <InputGroup.Text>Absolute Threshold</InputGroup.Text>
          <InputGroup.Text>Low</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/abs/low" type="number"/>
          <InputGroup.Text>High</InputGroup.Text>
          <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/thresholds/abs/high" type="number"/>
          </InputGroup>

          <EndpointButton endpoint={histogramEndpoint} event_type="click" fullpath="config/thresholds/setup" value={true}>
          Apply Changes
          </EndpointButton>
          </Stack>
        </TitleCard>
        </Col>
        <Col>
        <Row>
        <Col>
        <TitleCard title="Cluster Options">
          <Row>
          <Col>
          <InputGroup>
            <InputGroup.Text>Cluster Mode</InputGroup.Text>
            <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/cluster/mode/selected"
              buttonText={histogramEndpoint.data?.config?.cluster?.mode?.selected || "Unknown"}>

              {histogramEndpoint.data?.config?.cluster?.mode?.options ? histogramEndpoint.data.config.cluster.mode.options.map(
                (selection, index) => (
                  <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                )) : <></>
              }
            </EndpointDropdown>
          </InputGroup>
          </Col>
          <Col>
          <InputGroup>
          <InputGroup.Text>Cluster Type</InputGroup.Text>
            <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/cluster/type/selected"
              buttonText={histogramEndpoint.data?.config?.cluster?.type?.selected || "Unknown"}>

              {histogramEndpoint.data?.config?.cluster?.type?.options ? histogramEndpoint.data.config.cluster.type.options.map(
                (selection, index) => (
                  <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                )) : <></>
              }
            </EndpointDropdown>
          </InputGroup>
          </Col>
          </Row>
        </TitleCard>
        </Col>
        <Col xs={4}>
          <TitleCard title="Mapped Mode">
            <Stack>
            <InputGroup>
              <InputGroup.Text>Mode</InputGroup.Text>
              <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/mapped_mode/selected"
                buttonText={histogramEndpoint.data?.config?.mapped_mode?.selected || "Unknown"}>
                  {histogramEndpoint.data?.config?.mapped_mode?.options ? histogramEndpoint.data.config.mapped_mode.options.map(
                    (selection, index) => (
                      <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                    )) : <></>
                  }
                </EndpointDropdown>
            </InputGroup>
            </Stack>
          </TitleCard>
        </Col>
      </Row>
      <Row>
        <Col>
        <TitleCard title="Baseline Subtraction">
          <Stack direction="horizontal" gap={1}>
            <InputGroup>
              <InputGroup.Text>Mask</InputGroup.Text>
              <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/baseline/bsubmask/selected"
                buttonText={histogramEndpoint.data?.config?.baseline?.bsubmask?.selected || "Unknown"}>
                  {histogramEndpoint.data?.config?.baseline?.bsubmask?.options ? histogramEndpoint.data.config.baseline.bsubmask.options.map(
                  (selection, index) => (
                    <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                  )) : <></>
                }
                </EndpointDropdown>
            </InputGroup>
            <InputGroup>
              <InputGroup.Text>Divide</InputGroup.Text>
              <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/baseline/bsubDivide/selected"
                buttonText={histogramEndpoint.data?.config?.baseline?.bsubDivide?.selected || "Unknown"}>
                  {histogramEndpoint.data?.config?.baseline?.bsubDivide?.options ? histogramEndpoint.data.config.baseline.bsubDivide.options.map(
                  (selection, index) => (
                    <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                  )) : <></>
                }
                </EndpointDropdown>
            </InputGroup>
          </Stack>
        </TitleCard>
        </Col>
      </Row>
      </Col>
      </Row>
      <Row>
        <Col>
          <TitleCard title="Calibration Files">
            <Stack gap={1}>
              <InputGroup>
                <InputGroup.Text>CShare Ascii</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/CShareAscii/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/CShareAscii/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.CShareAscii?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.CShareAscii?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>CShare MC</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/CShareMC/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/CShareMC/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.CShareMC?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.CShareMC?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>CShare L3</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/CShareL3/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/CShareL3/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.CShareL3?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.CShareL3?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>linearity Gain HDF5</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/linearityGainHDF5/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/linearityGainHDF5/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.linearityGainHDF5?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.linearityGainHDF5?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>Gain Ascii</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/gainAscii/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/gainAscii/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.gainAscii?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.gainAscii?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>Linearity Ascii</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/settings_files/linearityAscii/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/settings_files/linearityAscii/activate"
                                checked={histogramEndpoint.data?.config?.settings_files?.linearityAscii?.activate || false}
                                value={histogramEndpoint.data?.config?.settings_files?.linearityAscii?.activate || false}
                />
              </InputGroup>
            </Stack>
          </TitleCard>
        </Col>
        <Col>
        <Stack gap={1}>
          <TitleCard title="Output Files">
              <Stack gap={1}>
              <InputGroup>
                <InputGroup.Text>HDF5</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/save_files/saveHdf5/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/save_files/saveHdf5/activate"
                                checked={histogramEndpoint.data?.config?.save_files?.saveHdf5?.activate || false}
                                value={histogramEndpoint.data?.config?.save_files?.saveHdf5?.activate || false}
                />
              </InputGroup>
              <InputGroup>
                <InputGroup.Text>Settings</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/save_files/save_settings/filename"/>
                <EndpointToggle endpoint={histogramEndpoint} event_type="click" label=""
                                fullpath="config/save_files/save_settings/activate"
                                checked={histogramEndpoint.data?.config?.save_files?.save_settings?.activate || false}
                                value={histogramEndpoint.data?.config?.save_files?.save_settings?.activate || false}
                />
              </InputGroup>
              </Stack>
          </TitleCard>
          <TitleCard title="UDP">
            <Stack gap={1}>
            <InputGroup>
                <InputGroup.Text>Send</InputGroup.Text>
                <EndpointDropdown endpoint={histogramEndpoint} event_type="select" fullpath="config/udp/send_udp/selected"
                  buttonText={histogramEndpoint.data.config?.udp?.send_udp?.selected || "Unknown"}>
                    {histogramEndpoint.data?.config?.udp?.send_udp?.options ? histogramEndpoint.data.config.udp.send_udp.options.map(
                      (selection, index) => (
                        <Dropdown.Item eventKey={selection} key={index}>{selection}</Dropdown.Item>
                      )) : <></>
                    }
                </EndpointDropdown>
            </InputGroup>
            <InputGroup>
                <InputGroup.Text>Rx</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/udp/rx" disabled={true}/>
            </InputGroup>
            <InputGroup>
                <InputGroup.Text>Tx</InputGroup.Text>
                <EndpointInput endpoint={histogramEndpoint} event_type="change" fullpath="config/udp/tx" disabled={true}/>
            </InputGroup>
            </Stack>
          </TitleCard>
          </Stack>
        </Col>
      </Row>
      </>
    </OdinApp>
  )
}

export default App
