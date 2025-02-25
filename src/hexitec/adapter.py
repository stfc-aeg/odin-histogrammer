from odin.adapters.adapter import (ApiAdapter, ApiAdapterResponse, response_types, wants_metadata)
from odin.adapters.parameter_tree import ParameterTreeError
from odin.util import decode_request_body


from .controller import HexitecController
# from hexitec.hexitec import Hexitec

import logging

class HexitecException(Exception):

    pass

class HexitecAdapter(ApiAdapter):

    hexitec_control = HexitecController

    def __init__(self, **kwargs):

        super(HexitecAdapter, self).__init__(**kwargs)

        logging.debug(self.options)
        self.controller = self.hexitec_control(self.options)

        # self.param_tree = ParameterTree(self.xdma_control._params)
        self.controller.init_tree()

    @response_types('application/json', default='application/json')
    def get(self, path, request):
        metadata = wants_metadata(request)
        try:
            response = self.controller.param_tree.get(path, metadata)
            content_type = 'application/json'
            status = 200
        except ParameterTreeError as param_error:
            response = {"response": "adxdma GET Error: {}".format(param_error)}
            content_type = 'application/json'
            status = 400

        # except AdxdmaException as xdma_err:
        #     response = {"response": "Adxdma API Error: {}".format(xdma_err.message)}
        #     content_type = "application/json"
        #     status = 400

        return ApiAdapterResponse(response, content_type=content_type, status_code=status)

    @response_types('application/json', default='application/json')
    def put(self, path, request):
        try:
            data = decode_request_body(request)
            self.controller.param_tree.set(path, data)

            response = self.controller.param_tree.get(path)
            content_type = 'application/json'
            status = 200

        except ParameterTreeError as param_error:
            response = {'response': 'adxdma PUT error: {}'.format(param_error)}
            content_type = 'application/json'
            status = 400

        except HexitecException as xdma_err:
            response = {'response': 'Adxdma API Error: {}'.format(xdma_err.message)}
            content_type = 'application/json'
            status = 400

        return ApiAdapterResponse(response, content_type=content_type, status_code=status)