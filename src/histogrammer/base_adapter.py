import logging

from odin.adapters.adapter import (
    ApiAdapter,
    ApiAdapterResponse,
    request_types,
    response_types,
    wants_metadata
)
from odin.util import decode_request_body

from .base_controller import BaseController, BaseError


class BaseAdapter(ApiAdapter):
    controller_cls = BaseController
    error_cls = BaseError

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._name = type(self).__name__
        self.controller = self.controller_cls(self.options)
        logging.debug("%s loaded", self._name)

    def initialize(self, adapters):
        
        adapters = {name: adapter for name, adapter in adapters.items() if adapter is not self}
        logging.debug("%s initialise called with %d adapters", self._name, len(adapters))
        self.controller.initialize(adapters)

    def cleanup(self):

        logging.debug("%s cleanip called", self._name)
        self.controller.cleanup()
        
    @response_types("application/json", default="application/json")
    def get(self, path, request):
        """Handle an HTTP GET request.

        This method handles an HTTP GET request, returning a JSON response.

        :param path: URI path of request
        :param request: HTTP request object
        :return: an ApiAdapterResponse object containing the appropriate response
        """
        try:
            response = self.controller.get(path, wants_metadata(request))
            status_code = 200
        except self.error_cls as error:
            response = {"error": str(error)}
            status_code = 400

        content_type = "application/json"

        return ApiAdapterResponse(response, content_type=content_type, status_code=status_code)
    
    @request_types("application/json", "application/vnd.odin-native")
    @response_types("application/json", default="application/json")
    def put(self, path, request):
        """Handle an HTTP PUT request.

        This method handles an HTTP PUT request, returning a JSON response.

        :param path: URI path of request
        :param request: HTTP request object
        :return: an ApiAdapterResponse object containing the appropriate response
        """

        content_type = "application/json"

        try:
            data = decode_request_body(request)
            self.controller.set(path, data)
            response = self.controller.get(path)
            status_code = 200
        except self.error_cls as error:
            response = {"error": str(error)}
            status_code = 400
        except (TypeError, ValueError) as error:
            response = {"error": "Failed to decode PUT request body: {}".format(str(error))}
            status_code = 400

        return ApiAdapterResponse(response, content_type=content_type, status_code=status_code)